import pytest
from pathlib import Path

@pytest.fixture
def linear_regression_data():
    """
    Fixture to provide sample data for linear regression tests.
    Returns a dictionary with sample data.
    """
    import pandas as pd

    file_path = Path('testing/linear_regression_data.csv')
    linear_regression_data = pd.read_csv(file_path)

    return linear_regression_data