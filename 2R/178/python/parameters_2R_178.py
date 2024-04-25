"""
This module contains the custom 
class for parameter calibration of model 2R 178.
"""

from gcubed.model_parameters.parameters import Parameters
from gcubed.data.database import Database


class Parameters2R178(Parameters):
    """
    This class does customised parameter calibration for model 2R 170 logv7.
    """

    def __init__(self, database: Database, base_year: int) -> None:
        """

        ### Overview

        No custom parameter calibrations are required by this model version.

        ### Arguments

        `database`: The database used to calibrate the parameters.

        `base_year`: The base year that the database needs to be rebased to before
        calibration of the parameters.

        """
        super().__init__(database=database, base_year=base_year)

        # Required statements in all final subclasses of the parameter class.
        self._parameter_values.index = self._parameter_full_names
        self.validate()

    def validate(self):
        super().validate()
