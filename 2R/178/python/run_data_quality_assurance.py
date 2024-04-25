"""
Run the baseline but only resolve the model when needed.
"""

from email.mime import base
import os
import pickle
import logging
import numpy as np
import pandas as pd
import gcubed
from gcubed.model import Model
from gcubed.projections.baseline_projections import BaselineProjections
from gcubed.linearisation.solved_model import SolvedModel
from gcubed.model_configuration import ModelConfiguration
from gcubed.sym_data import SymData
from model_constants import CONFIGURATION_FILE

# -------------------------------------------------------------------------------
# Configuration details start here
# -------------------------------------------------------------------------------
# Set the last year for QA checks
last_year: str = "2021"

# Set the flag to use the solved model if available
using_solved_model_if_available: bool = False

# Start projections from neutral real interest rate
start_from_neutral_real_interest_rate: bool = True

# -------------------------------------------------------------------------------
# Configuration details end here
# -------------------------------------------------------------------------------

# Configure the output options for numpy matrices.
np.set_printoptions(suppress=True, linewidth=10000)
np.set_printoptions(formatter={"float": lambda x: "{0:0.6f}".format(x)})

print("Loading model config")
# Create the model configuration just so we can get the model version and build.
configuration: ModelConfiguration = ModelConfiguration(
    configuration_file=CONFIGURATION_FILE,
)

# Get the timestamp for the results
timestamp: str = gcubed.now()

# Initialise the run
try:
    results_directory: str = os.path.join(
        os.getcwd(),
        "results",
        configuration.version,
        configuration.build,
        f"{os.path.splitext(os.path.basename(__file__))[0]}",
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

# Load or create the solved model first
solved_model: SolvedModel = None
solved_model_pickle_file: str = os.path.join(results_directory, "solved_model.pickle")
if os.path.exists(solved_model_pickle_file) and using_solved_model_if_available:
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
    solved_model=solved_model,
    start_from_neutral_real_interest_rate=start_from_neutral_real_interest_rate,
)
baseline_projections_pickle_file: str = os.path.join(
    results_directory, "baseline_projections.pickle"
)


def check_equality(a: pd.DataFrame, b: pd.DataFrame, atol=1e-4, rtol=1e-4) -> bool:
    """
    Check that the two dataframes are equal, within reasonable levels of precision.

    ### Arguments

    - `a` : DataFrame : The first dataframe to compare.
    - `b` : DataFrame : The second dataframe to compare.
    - `atol` : float : The absolute tolerance between the elements. Defaults to 1e-8.
    - `rtol` : float : The relative tolerance between the elements. Defaults to 1e-5.

    ### Returns

    - `True` if the dataframes are equal.
    """
    tests: np.array = np.isclose(a.to_numpy(), b.to_numpy(), atol=atol, rtol=rtol)

    # Get the prefix of the first index value from a
    a_prefix = a.index[0].split("(")[0]
    b_prefix = b.index[0].split("(")[0]

    if tests.all().all():
        logging.info(f"{a_prefix} and {b_prefix} match.")
        return True

    # Step 1: Re-identify the INTR and INTF rows for clarity in logging
    a_rows = a.index
    b_rows = b.index

    assert a.shape == b.shape, "DataFrames have different shapes."

    # Since you want to log the exact locations of mismatches,
    # find the False positions in the 'tests' array
    mismatches = np.where(~tests)

    # Finding the False positions in the 'tests' array for mismatches
    mismatch_indices = np.where(~tests)

    for row, col in zip(*mismatch_indices):
        a_variable = a.index[row]
        b_variable = b.index[row]
        a_value = a.iloc[row, col]
        b_value = b.iloc[row, col]
        a_year = a.columns[col]
        b_year = b.columns[col]
        logging.warning(
            f"Mismatch: {a_variable}[{a_year}]={a_value:.4f} and {b_variable}[{b_year}]={b_value:.4f}"
        )

    # Vertically concatenate the a and b DataFrames.
    combined = pd.concat([a, b], axis=0)
    combined.to_csv(os.path.join(results_directory, f"{a_prefix}_vs_{b_prefix}.csv"))

    return False


# Access the combined database and database baseline projections.
baseline_projections.annotated_combined_database_and_projections.to_csv(
    os.path.join(results_directory, "combined_database_and_projections.csv")
)

baseline_projections.charting_projections.to_csv(
    os.path.join(results_directory, "baseline_projections.csv")
)

data: pd.DataFrame = baseline_projections.combined_database_and_projections
names = data.index.str
sym_data: SymData = baseline_projections.sym_data
variables: pd.DataFrame = sym_data.variable_summary

# Get various lists of years (columns)
all_years: list[str] = list(data.columns)
t_years: list[str] = all_years[1:-1]
t_plus_1_years: list[str] = all_years[2:]
t_minus_1_years: list[str] = all_years[:-2]
database_years: list[str] = baseline_projections.model.database.get_year_labels
first_projection_year: str = str(
    baseline_projections.configuration.original_first_projection_year
)

# Get the index of '2021' in the list of current_years
last_year_index: int = t_years.index(last_year)
t_years: list[str] = t_years[: last_year_index + 1]
t_plus_1_years: list[str] = t_plus_1_years[: last_year_index + 1]
t_minus_1_years: list[str] = t_minus_1_years[: last_year_index + 1]

# # Check bond accumulation
# if (
#     sym_data.has_variables("BOND")
#     and sym_data.has_variables("DEFI")
#     and sym_data.has_variables("PBAG")
# ):
#     labgrow: pd.DataFrame = baseline_projections.model.parameters.parameter(
#         parameter_name="labgrow"
#     ).transpose()
#     BOND: pd.DataFrame = data.loc[names.startswith("BOND("), t_minus_1_years]
#     DEFI: pd.DataFrame = data.loc[names.startswith("DEFI("), t_minus_1_years]
#     PBAG: pd.DataFrame = data.loc[names.startswith("PBAG("), t_minus_1_years]
#     calcBOND: pd.DataFrame = BOND.copy()
#     # iterate over the second year to the last year:
#     previous_year: str = None
#     for year in t_minus_1_years:
#         if previous_year is None:
#             previous_year = year
#             continue
#         # Calculate the bond accumulation for each year.
#         calcBOND[year] = (
#             (calcBOND.loc[:, [previous_year]].to_numpy() * (1 - labgrow.to_numpy()))
#             + DEFI.loc[:, [previous_year]].to_numpy()
#             - PBAG.loc[:, [previous_year]].to_numpy()
#         )
#         previous_year = year
#     calcBOND.index = BOND.index.str.replace("BOND", "calcBOND")
#     # Get the change in the bonds from the current to the future year.
#     check_equality(a=BOND, b=calcBOND)

# # Check capital accumulation
# # lead(CAP)  = JNV  + (1-delta-labgrow)*CAP
# if sym_data.has_variables("CAP") and sym_data.has_variables("JNV"):
#     labgrow: pd.DataFrame = baseline_projections.model.parameters.parameter(
#         parameter_name="labgrow"
#     ).transpose()
#     labgrow: np.array = np.tile(
#         labgrow.to_numpy(),
#         (baseline_projections.sym_data.non_electricity_distribution_sectors_count, 1),
#     )
#     delta: pd.DataFrame = baseline_projections.model.parameters.parameter(
#         parameter_name="delta"
#     ).transpose()
#     delta: np.array = np.tile(
#         delta.to_numpy(),
#         (baseline_projections.sym_data.non_electricity_distribution_sectors_count, 1),
#     )
#     CAP: pd.DataFrame = data.loc[names.startswith("CAP("), t_minus_1_years]
#     JNV: pd.DataFrame = data.loc[names.startswith("JNV("), t_minus_1_years]
#     calcCAP: pd.DataFrame = CAP.copy()
#     # iterate over the second year to the last year:
#     previous_year: str = None
#     for year in t_minus_1_years:
#         if previous_year is None:
#             previous_year = year
#             continue
#         # Calculate the capital accumulation for each year.
#         calcCAP[year] = JNV.loc[:, [year]].to_numpy() + (
#             (1 - delta - labgrow) * CAP.loc[:, [previous_year]].to_numpy()
#         )
#         previous_year = year
#     calcCAP.index = CAP.index.str.replace("CAP", "calcCAP")
#     check_equality(a=CAP, b=calcCAP)


# # Check financial asset accumulation
# if (
#     sym_data.has_variables("ASSE")
#     and sym_data.has_variables("CURR")
#     and sym_data.has_variables("ABUY")
#     and sym_data.has_variables("REXN")
# ):
#     ashr: pd.DataFrame = baseline_projections.model.parameters.parameter(
#         parameter_name="ashr"
#     ).transpose()
#     ashr: np.array = ashr.stack().reset_index(drop=True).to_frame().to_numpy()
#     aeye: pd.DataFrame = baseline_projections.model.parameters.parameter(
#         parameter_name="aeye"
#     ).transpose()
#     aeye: np.array = aeye.stack().reset_index(drop=True).to_frame().to_numpy()
#     labgrow: pd.DataFrame = baseline_projections.model.parameters.parameter(
#         parameter_name="labgrow"
#     ).transpose()
#     labgrow: np.array = np.tile(
#         labgrow.to_numpy(), (baseline_projections.sym_data.regions_count, 1)
#     )
#     ASSE: pd.DataFrame = data.loc[names.startswith("ASSE("), t_minus_1_years]
#     CURR: pd.DataFrame = data.loc[names.startswith("CURR("), t_minus_1_years]
#     ABUY: pd.DataFrame = data.loc[names.startswith("ABUY("), t_minus_1_years]
#     REXN: pd.DataFrame = data.loc[names.startswith("REXN("), t_minus_1_years]

#     ABUY: np.array = pd.DataFrame(
#         np.tile(ABUY.to_numpy(), (baseline_projections.sym_data.regions_count, 1))
#     )
#     ABUY.columns = t_minus_1_years
#     CURR: np.array = pd.DataFrame(
#         np.tile(CURR.to_numpy(), (baseline_projections.sym_data.regions_count, 1))
#     )
#     CURR.columns = t_minus_1_years
#     REXN: np.array = pd.DataFrame(
#         np.repeat(REXN.to_numpy(), baseline_projections.sym_data.regions_count, axis=0)
#     )
#     REXN.columns = t_minus_1_years

#     calcASSE: pd.DataFrame = ASSE.copy()
#     # iterate over the second year to the last year:
#     previous_year: str = None
#     for year in t_minus_1_years:
#         if previous_year is None:
#             previous_year = year
#             continue
#         # Calculate the asset accumulation for each year.
#         # lead(ASSE) = ASSE*( 1-labgrow(owner) )
#         #        + ( ashr*ABUY + aeye*(CURR(owner)-ABUY) ) / exp(REXN(currency)) ;

#         calcASSE[year] = (
#             calcASSE.loc[:, [previous_year]].to_numpy() * (1 - labgrow)
#         ) + (
#             ashr * ABUY.loc[:, [previous_year]].to_numpy()
#             + aeye
#             * (
#                 CURR.loc[:, [previous_year]].to_numpy()
#                 - ABUY.loc[:, [previous_year]].to_numpy()
#             )
#         ) / REXN.loc[
#             :, [previous_year]
#         ].to_numpy()
#         previous_year = year
#     calcASSE.index = ASSE.index.str.replace("ASSE", "calcASSE")

#     # Get the change in the assets from the current to the future year.
#     check_equality(a=ASSE, b=calcASSE)

# # Check trade balances

# # Create the benchmark for comparison.
# zeros: pd.DataFrame = pd.DataFrame(np.zeros((1, len(t_years))))
# zeros.index = ["ZEROS()"]
# zeros.columns = t_years

# # Check that the TBAL variables summed across regions equal zero.
# if sym_data.has_variables("TBAL") and sym_data.has_variables("GDPR"):
#     TBAL: pd.DataFrame = data.loc[names.startswith("TBAL("), t_years]
#     GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years]
#     TBAL.loc[:, :] *= GDPR.to_numpy()

#     # Create a dataframe with a single row containing the column sums of TBAL.
#     globalTBAL: pd.DataFrame = pd.DataFrame(TBAL.sum(axis=0)).transpose()
#     globalTBAL.index = ["globalTBAL()"]
#     # Check that the sum of the TBAL variables is zero using the check_equality function.
#     check_equality(
#         a=globalTBAL,
#         b=zeros,
#     )

# if sym_data.has_variables("TBAU") and sym_data.has_variables("GDPR"):
#     TBAU: pd.DataFrame = data.loc[names.startswith("TBAU("), t_years] / 100
#     GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years] / 100
#     TBAU.loc[:, :] *= GDPR.to_numpy()

#     # Create a dataframe with a single row containing the column sums of TBAU.
#     globalTBAU: pd.DataFrame = pd.DataFrame(TBAU.sum(axis=0)).transpose()
#     globalTBAU.index = ["globalTBAU()"]
#     # Check that the sum of the TBAU variables is zero using the check_equality function.
#     check_equality(
#         a=globalTBAU,
#         b=zeros,
#     )

# # Check that the sum of the IRAS values across regions is zero.
# if sym_data.has_variables("IRAS") and sym_data.has_variables("GDPR"):
#     IRAS: pd.DataFrame = data.loc[names.startswith("IRAS("), t_years] / 100
#     GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years] / 100
#     IRAS.loc[:, :] *= GDPR.to_numpy()

#     # Create a dataframe with a single row containing the column sums of IRAS.
#     globalIRAS: pd.DataFrame = pd.DataFrame(IRAS.sum(axis=0)).transpose()
#     globalIRAS.index = ["globalIRAS()"]
#     # Check that the sum of the IRAS variables is zero using the check_equality function.
#     check_equality(
#         a=globalIRAS,
#         b=zeros,
#     )

# # Check that the sum of the CURR values across regions is zero
# if sym_data.has_variables("CURR") and sym_data.has_variables("GDPR"):
#     CURR: pd.DataFrame = data.loc[names.startswith("CURR("), t_years] / 100
#     GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years] / 100
#     CURR.loc[:, :] *= GDPR.to_numpy()

#     # Create a dataframe with a single row containing the column sums of CURR.
#     globalCURR: pd.DataFrame = pd.DataFrame(CURR.sum(axis=0)).transpose()
#     globalCURR.index = ["globalCURR()"]
#     # Check that the sum of the CURR variables is zero using the check_equality function.
#     check_equality(
#         a=globalCURR,
#         b=zeros,
#     )

# # Check that the sum of the CURN values across regions is zero
# if sym_data.has_variables("CURN") and sym_data.has_variables("GDPR"):
#     CURN: pd.DataFrame = data.loc[names.startswith("CURN("), t_years] / 100
#     GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years] / 100
#     CURN.loc[:, :] *= GDPR.to_numpy()

#     # Create a dataframe with a single row containing the column sums of CURN.
#     globalCURN: pd.DataFrame = pd.DataFrame(CURN.sum(axis=0)).transpose()
#     globalCURN.index = ["globalCURN()"]
#     # Check that the sum of the CURN variables is zero using the check_equality function.
#     check_equality(
#         a=globalCURN,
#         b=zeros,
#     )

# # Check that the sum of the EXQT values across regions equals the sum of the IMQT values across regions
# if (
#     sym_data.has_variables("EXQT")
#     and sym_data.has_variables("IMQT")
#     and sym_data.has_variables("GDPR")
# ):
#     EXQT: pd.DataFrame = data.loc[names.startswith("EXQT("), t_years] / 100
#     IMQT: pd.DataFrame = data.loc[names.startswith("IMQT("), t_years] / 100
#     GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years] / 100
#     EXQT.loc[:, :] *= GDPR.to_numpy()
#     IMQT.loc[:, :] *= GDPR.to_numpy()

#     # Create a dataframe with a single row containing the column sums of EXQT and IMQT.
#     globalEXQT: pd.DataFrame = pd.DataFrame(EXQT.sum(axis=0)).transpose()
#     globalIMQT: pd.DataFrame = pd.DataFrame(IMQT.sum(axis=0)).transpose()
#     globalEXQT.index = ["globalEXQT()"]
#     globalIMQT.index = ["globalIMQT()"]
#     # Check that the sum of the EXQT variables equals the sum of the IMQT variables using the check_equality function.
#     check_equality(
#         a=globalEXQT,
#         b=globalIMQT,
#     )


# Check WAGE vs WAGG
if sym_data.has_variables("WAGE") and sym_data.has_variables("WAGG"):
    WAGE: pd.DataFrame = data.loc[names.startswith("WAGE("), t_years]
    WAGG: pd.DataFrame = data.loc[names.startswith("WAGG("), t_years]
    check_equality(
        a=WAGE,
        b=WAGG,
    )

# Check PRCT against PRCL
if sym_data.has_variables("PRCT") and sym_data.has_variables("PRCL"):
    check_equality(
        a=data.loc[names.startswith("PRCT("), t_years],
        b=data.loc[names.startswith("PRCL("), t_plus_1_years],
    )

# Check PRID against PRDL
if sym_data.has_variables("PRID") and sym_data.has_variables("PRDL"):
    check_equality(
        a=data.loc[names.startswith("PRID("), t_years],
        b=data.loc[names.startswith("PRDL("), t_plus_1_years],
    )

# Check PRCO against PRCOL
if sym_data.has_variables("PRCO") and sym_data.has_variables("PRCOL"):
    check_equality(
        a=data.loc[names.startswith("PRCO("), t_years],
        b=data.loc[names.startswith("PRCOL("), t_plus_1_years],
    )

# Check that INTR = INTF
if sym_data.has_variables("INTR") and sym_data.has_variables("INTF"):
    check_equality(
        a=data.loc[names.startswith("INTR("), t_years],
        b=data.loc[names.startswith("INTF("), t_years],
    )

# Check INTF = INTN - lead(PRID) + PRID
if (
    sym_data.has_variables("INTF")
    and sym_data.has_variables("INTN")
    and sym_data.has_variables("PRID")
):
    INTF: pd.DataFrame = data.loc[names.startswith("INTF("), t_years]
    calcINTF: pd.DataFrame = INTF.copy()
    calcINTF.loc[:, :] = data.loc[
        names.startswith("INTN("), t_years
    ].to_numpy() - 100 * (
        np.log(data.loc[names.startswith("PRID("), t_plus_1_years].to_numpy())
        - np.log(data.loc[names.startswith("PRID("), t_years].to_numpy())
    )
    calcINTF.index = calcINTF.index.str.replace("INTF", "calcINTF")
    check_equality(
        a=INTF,
        b=calcINTF,
    )

# Check INTN against INTL
if sym_data.has_variables("INTN") and sym_data.has_variables("INTL"):
    check_equality(
        a=data.loc[names.startswith("INTN("), t_years],
        b=data.loc[names.startswith("INTL("), t_plus_1_years],
    )

# Check INPL against INPN
if sym_data.has_variables("INPL") and sym_data.has_variables("INPN"):
    check_equality(
        a=data.loc[names.startswith("INPN("), t_years],
        b=data.loc[names.startswith("INPL("), t_plus_1_years],
    )

# Check INFP against PRID and PRDL
if (
    sym_data.has_variables("INFP")
    and sym_data.has_variables("PRID")
    and sym_data.has_variables("PRDL")
):
    INFP: pd.DataFrame = data.loc[names.startswith("INFP("), t_years]
    calcINFP: pd.DataFrame = INFP.copy()
    calcINFP.loc[:, :] = 100 * (
        np.log(data.loc[names.startswith("PRID("), t_years].to_numpy())
        - np.log(data.loc[names.startswith("PRDL("), t_years].to_numpy())
    )
    calcINFP.index = calcINFP.index.str.replace("INFP", "calcINFP")
    check_equality(
        a=INFP,
        b=calcINFP,
    )

# Check INFL against PRCT and PRCL
if (
    sym_data.has_variables("INFL")
    and sym_data.has_variables("PRCT")
    and sym_data.has_variables("PRCL")
):
    INFL: pd.DataFrame = data.loc[names.startswith("INFL("), t_years]
    calcINFL: pd.DataFrame = INFL.copy()
    calcINFL.loc[:, :] = 100 * (
        np.log(data.loc[names.startswith("PRCT("), t_years].to_numpy())
        - np.log(data.loc[names.startswith("PRCL("), t_years].to_numpy())
    )
    calcINFL.index = calcINFL.index.str.replace("INFL", "calcINFL")
    check_equality(
        a=INFL,
        b=calcINFL,
    )

# Check INFCL against PRCO and PRCOL
if (
    sym_data.has_variables("INFCL")
    and sym_data.has_variables("PRCO")
    and sym_data.has_variables("PRCOL")
):
    INFCL: pd.DataFrame = data.loc[names.startswith("INFCL("), t_years]
    calcINFCL: pd.DataFrame = INFCL.copy()
    calcINFCL.loc[:, :] = 100 * (
        np.log(data.loc[names.startswith("PRCO("), t_years].to_numpy())
        - np.log(data.loc[names.startswith("PRCOL("), t_years].to_numpy())
    )
    calcINFCL.index = calcINFCL.index.str.replace("INFCL", "calcINFCL")
    check_equality(
        a=INFCL,
        b=calcINFCL,
    )

# Check similarity of INFP and INFL inflation rates
if sym_data.has_variables("INFL") and sym_data.has_variables("INFP"):
    check_equality(
        a=data.loc[names.startswith("INFL("), t_years],
        b=data.loc[names.startswith("INFP("), t_years],
        atol=1e-1,
        rtol=1e-1,
    )

# Check similarity of INFL and INFCL inflation rates
if sym_data.has_variables("INFL") and sym_data.has_variables("INFCL"):
    check_equality(
        a=data.loc[names.startswith("INFL("), t_years],
        b=data.loc[names.startswith("INFCL("), t_years],
        atol=1e-1,
        rtol=1e-1,
    )

# Check similarity of INFP and INFCL inflation rates
if sym_data.has_variables("INFP") and sym_data.has_variables("INFCL"):
    check_equality(
        a=data.loc[names.startswith("INFP("), t_years],
        b=data.loc[names.startswith("INFCL("), t_years],
        atol=1e-1,
        rtol=1e-1,
    )

# Check similarity of INFP and wage inflation
if sym_data.has_variables("INFP") and sym_data.has_variables("WAGE"):
    INFP: pd.DataFrame = data.loc[names.startswith("INFP("), t_years]
    wageINFL: pd.DataFrame = INFP.copy()
    wageINFL.loc[:, :] = 100 * (
        np.log(data.loc[names.startswith("WAGE("), t_years].to_numpy())
        - np.log(data.loc[names.startswith("WAGE("), t_minus_1_years].to_numpy())
    )
    wageINFL.index = wageINFL.index.str.replace("INFP", "wageINFL")
    check_equality(
        a=INFP,
        b=wageINFL,
    )

# Check OUTL = lag(OUTP)
if sym_data.has_variables("OUTP") and sym_data.has_variables("OUTL"):
    check_equality(
        a=data.loc[names.startswith("OUTP("), t_years],
        b=data.loc[names.startswith("OUTL("), t_plus_1_years],
    )
logging.info("Skipping remaining tests.")
exit(0)

# Check LAM values
# LEAD(LAM) = (1+INTR+RISE+delta)*LAM
#           - (1-TCOR)*exp(PRK-PRID)
#           - (1-TITC)*exp(PRII-PRID)*(phi/2)*(JNV/CAP)^2 ;
if (
    sym_data.has_variables("LAM")
    and sym_data.has_variables("INTR")
    and sym_data.has_variables("RISE")
    and sym_data.has_variables("PRK")
    and sym_data.has_variables("PRII")
    and sym_data.has_variables("PRID")
    and sym_data.has_variables("TCOR")
    and sym_data.has_variables("TITC")
):
    LAM: pd.DataFrame = data.loc[names.startswith("LAM("), t_plus_1_years]
    lagLAM: pd.DataFrame = data.loc[names.startswith("LAM("), t_years] / 100
    INTR: pd.DataFrame = data.loc[names.startswith("INTR("), t_years] / 100
    RISE: pd.DataFrame = data.loc[names.startswith("RISE("), t_years] / 100
    PRK: pd.DataFrame = data.loc[names.startswith("PRK("), t_years] / 100
    PRII: pd.DataFrame = data.loc[names.startswith("PRII("), t_years] / 100
    PRID: pd.DataFrame = data.loc[names.startswith("PRID("), t_years] / 100
    JNV: pd.DataFrame = data.loc[names.startswith("JNV("), t_years] / 100
    CAP: pd.DataFrame = data.loc[names.startswith("CAP("), t_years] / 100
    TCOR: pd.DataFrame = data.loc[names.startswith("TCOR("), t_years] / 100
    TITC: pd.DataFrame = data.loc[names.startswith("TITC("), t_years] / 100
    delta: pd.DataFrame = baseline_projections.model.parameters.parameter(
        parameter_name="delta"
    ).transpose()
    delta: np.array = np.tile(
        delta.to_numpy(),
        (baseline_projections.sym_data.non_electricity_distribution_sectors_count, 1),
    )
    phi: pd.DataFrame = baseline_projections.model.parameters.parameter(
        parameter_name="phi"
    ).transpose()
    phi: np.array = phi.stack().reset_index(drop=True).to_frame().to_numpy()
    INTR: np.array = np.tile(
        INTR.to_numpy(),
        (baseline_projections.sym_data.non_electricity_distribution_sectors_count, 1),
    )
    TCOR: np.array = np.tile(
        TCOR.to_numpy(),
        (baseline_projections.sym_data.non_electricity_distribution_sectors_count, 1),
    )
    PRID: np.array = np.tile(
        PRID.to_numpy(),
        (baseline_projections.sym_data.non_electricity_distribution_sectors_count, 1),
    )
    PRII: np.array = np.tile(
        PRII.to_numpy(),
        (baseline_projections.sym_data.non_electricity_distribution_sectors_count, 1),
    )
    calcLAM: pd.DataFrame = LAM.copy() * 0.0
    calcLAM.loc[:, :] = (
        (1 + INTR + RISE.to_numpy() + delta) * lagLAM.to_numpy()
        - (1 - TCOR) * (PRK.to_numpy() / PRID)
        - (1 - TITC.to_numpy())
        * (PRII / PRID)
        * (phi / 2)
        * (JNV.to_numpy() / CAP.to_numpy()) ** 2
    ) * 100.0
    check_equality(
        a=LAM,
        b=calcLAM,
    )


# Check EPRC against INFL
if sym_data.has_variables("EPRC") and sym_data.has_variables("INFL"):
    check_equality(
        a=data.loc[names.startswith("INFL("), t_years],
        b=data.loc[names.startswith("EPRC("), t_minus_1_years],
    )

# Check EPRC = lead(PRCT) - PRCT
if sym_data.has_variables("EPRC") and sym_data.has_variables("PRCT"):
    EPRC: pd.DataFrame = data.loc[names.startswith("EPRC("), t_years]
    calcEPRC: pd.DataFrame = EPRC.copy()
    calcEPRC.loc[:, :] = 100 * (
        np.log(data.loc[names.startswith("PRCT("), t_plus_1_years].to_numpy())
        - np.log(data.loc[names.startswith("PRCT("), t_years].to_numpy())
    )
    calcEPRC.index = calcEPRC.index.str.replace("EPRC", "calcEPRC")
    check_equality(
        a=EPRC,
        b=calcEPRC,
    )


# Check that YGRO = ln(OUTP) - ln(OUTL)
if (
    sym_data.has_variables("YGRO")
    and sym_data.has_variables("GDPR")
    and sym_data.has_variables("OUTP")
):
    YGRO: pd.DataFrame = data.loc[names.startswith("YGRO("), t_years]
    GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years]
    GDPLag: pd.DataFrame = data.loc[names.startswith("GDPR("), t_minus_1_years]
    OUTP: pd.DataFrame = data.loc[names.startswith("OUTP("), t_years] * GDPR.to_numpy()
    OUTLag: pd.DataFrame = (
        data.loc[names.startswith("OUTL("), t_years] * GDPLag.to_numpy()
    )
    calcYGRO: pd.DataFrame = YGRO.copy()
    calcYGRO.loc[:, :] = 100 * (np.log(OUTP.to_numpy()) - np.log(OUTLag.to_numpy()))
    calcYGRO.index = calcYGRO.index.str.replace("YGRO", "calcYGRO")
    check_equality(
        a=YGRO,
        b=calcYGRO,
    )

# Check EYGR against OUTP and the lead of OUTP
if (
    sym_data.has_variables("EYGR")
    and sym_data.has_variables("GDPR")
    and sym_data.has_variables("OUTP")
):
    EYGR: pd.DataFrame = data.loc[names.startswith("EYGR("), t_years]
    GDPR: pd.DataFrame = data.loc[names.startswith("GDPR("), t_years]
    GDPLead: pd.DataFrame = data.loc[names.startswith("GDPR("), t_plus_1_years]
    OUTP: pd.DataFrame = data.loc[names.startswith("OUTP("), t_years] * GDPR.to_numpy()
    OUTLead: pd.DataFrame = (
        data.loc[names.startswith("OUTP("), t_plus_1_years] * GDPLead.to_numpy()
    )
    calcEYGR: pd.DataFrame = EYGR.copy()
    calcEYGR.index = calcEYGR.index.str.replace("EYGR", "calcEYGR")
    calcEYGR.loc[:, :] = 100 * (np.log(OUTLead.to_numpy()) - np.log(OUTP.to_numpy()))
    check_equality(
        a=EYGR,
        b=calcEYGR,
    )

# Check EYGR against YGRO
if sym_data.has_variables("EYGR") and sym_data.has_variables("YGRO"):
    check_equality(
        a=data.loc[names.startswith("YGRO("), t_years],
        b=data.loc[names.startswith("EYGR("), t_minus_1_years],
    )
