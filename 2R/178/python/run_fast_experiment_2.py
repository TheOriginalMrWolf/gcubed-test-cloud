"""
Run the baseline but only resolve the model when needed.
"""
import os
import pickle
import logging
import numpy as np
import gcubed
from gcubed.model import Model
from gcubed.projections.baseline_projections import BaselineProjections
from gcubed.projections.projections import Projections
from gcubed.linearisation.solved_model import SolvedModel
from gcubed.model_configuration import ModelConfiguration
from gcubed.runners.simulation_runner import SimulationRunner
from model_constants import CONFIGURATION_FILE, EXPERIMENT_2 as EXPERIMENT

# Configure the output options for numpy matrices.
np.set_printoptions(suppress=True, linewidth=10000)
np.set_printoptions(formatter={"float": lambda x: "{0:0.6f}".format(x)})

# Create the model configuration just so we can get the model version and build.
configuration: ModelConfiguration = ModelConfiguration(
    configuration_file=CONFIGURATION_FILE,
)

# Get the name of this script
script_name: str = os.path.splitext(os.path.basename(__file__))[0]

# Get the timestamp for the results
timestamp: str = gcubed.now()

# Initialise the run
try:
    results_directory: str = os.path.join(
        os.getcwd(),
        "results",
        configuration.version,
        configuration.build,
        f"{script_name}",
    )
    if not os.path.exists(results_directory):
        os.makedirs(results_directory)
    logging.info(f"Created results directory {results_directory}")
except Exception as e:
    logging.error(f"Could not create results directory {results_directory}")
    raise e

# Create the results directory if it does not exist
if not os.path.exists(results_directory):
    os.makedirs(results_directory)

# Configure the logging system
formatter = logging.Formatter(
    "%(asctime)s - %(levelname)s - %(filename)s - Line:%(lineno)d - %(message)s"
)
logging.getLogger().handlers.clear()
logging.getLogger().setLevel(logging.DEBUG)
stream_handler = logging.StreamHandler()
stream_handler.setLevel(logging.DEBUG)
stream_handler.setFormatter(formatter)
logging.getLogger().addHandler(stream_handler)
log_file: str = os.path.join(results_directory, "run.log")
file_handler = logging.FileHandler(log_file)
file_handler.setLevel(logging.DEBUG)  # Set the level to DEBUG to capture all messages
file_handler.setFormatter(formatter)
logging.getLogger().addHandler(file_handler)
logging.info(f"Saving results and logs to {results_directory}.")

# Load the previously saved baseline projections, if they exist.
baseline_projections: BaselineProjections = None
baseline_projections_pickle_file: str = os.path.join(
    results_directory, "baseline_projections.pickle"
)
if os.path.exists(baseline_projections_pickle_file):
    logging.info(
        f"Loading previously generated baseline projections from {baseline_projections_pickle_file}"
    )
    with open(baseline_projections_pickle_file, "rb") as file:
        baseline_projections: BaselineProjections = pickle.load(file)


# Solve the model and regenerate the baseline if not already done.
if not (baseline_projections and isinstance(baseline_projections, BaselineProjections)):
    # Load or create the solved model first
    solved_model: SolvedModel = None
    solved_model_pickle_file: str = os.path.join(
        results_directory, "solved_model.pickle"
    )
    if os.path.exists(solved_model_pickle_file):
        logging.info(f"Loading previously solved model from {solved_model_pickle_file}")
        with open(solved_model_pickle_file, "rb") as file:
            solved_model: SolvedModel = pickle.load(file)

    if not (solved_model and isinstance(solved_model, SolvedModel)):
        model: Model = Model(configuration=configuration)
        solved_model: SolvedModel = SolvedModel(model=model)
        with open(solved_model_pickle_file, "wb") as file:
            pickle.dump(solved_model, file)
        logging.info(f"Saved the solved model for later reuse.")

    baseline_projections: BaselineProjections = BaselineProjections(
        solved_model=solved_model
    )
    with open(baseline_projections_pickle_file, "wb") as file:
        pickle.dump(baseline_projections, file)
    logging.info(f"Saved the baseline projections for later reuse.")


# Set up the simulation runner.
runner: SimulationRunner = SimulationRunner(
    baseline_projections=baseline_projections,
    experiment_design_file=EXPERIMENT,
)

# Run the simulation experiment.
runner.run()

# Save the raw baseline projections
baseline_projections: Projections = runner.baseline_projections
baseline_projections.charting_projections.to_csv(
    os.path.join(
        results_directory,
        f"{baseline_projections.name}.csv",
    )
)

# Save final projections
final_projections: Projections = runner.final_projections
final_projections.charting_projections.to_csv(
    os.path.join(
        results_directory,
        f"{final_projections.name}.csv",
    )
)

deviations = gcubed.projections.deviations(
    new_projections=final_projections,
    original_projections=baseline_projections,
)
deviations.to_csv(
    os.path.join(
        results_directory,
        f"deviations of {final_projections.name} from {baseline_projections.name}.csv",
    )
)
