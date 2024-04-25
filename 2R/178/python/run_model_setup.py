"""
Just set the model up - good for generating diagnostics etc.
"""
import logging
from gcubed.model import Model
from gcubed.model_configuration import ModelConfiguration
from model_constants import CONFIGURATION_FILE

# Create the model configuration just so we can get the model version and build.
configuration: ModelConfiguration = ModelConfiguration(
    configuration_file=CONFIGURATION_FILE,
)

# Configure the logging system
logging.getLogger().handlers.clear()
logging.getLogger().setLevel(logging.DEBUG)
stream_handler = logging.StreamHandler()
stream_handler.setLevel(logging.DEBUG)
stream_handler.setFormatter(
    logging.Formatter(
        "%(asctime)s - %(levelname)s - %(filename)s - Line:%(lineno)d - %(message)s"
    )
)

model: Model = Model(configuration=configuration)
