# Project Ambrose by Imjustchico
# The driver's own failures: a step that its condition never satisfied, and a refusal that names why the driver will not go on.


class DriverError(Exception):
    pass


class StepFailed(DriverError):
    pass


class Refused(DriverError):
    pass
