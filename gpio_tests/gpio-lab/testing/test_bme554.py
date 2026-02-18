from bme554 import unwrap_twos_complement

def test_read_hex_data():
    """
    Unit test for the read_hex_data function.
    """
    from bme554 import read_hex_data

    hex_data = read_hex_data('testing/hexdump.txt')

    expected_output = ["00", "01", "FF", "FF", "80", "00"]

    # Assert that the result matches the expected output
    # First 2 and last 2 bytes of the hexdump.txt file
    expected_output = ['99', '01', '71', '01']
    for i in [0, 1, -1, -2]:
        print(i)
        assert hex_data[i] == expected_output[i], f"Expected {expected_output[i]}, but got {hex_data[i]}"
    
def test_unwrap_twos_complent():
    """
    Unit test for the unwrap_twos_complent function.
    """
    # Test case with known input and expected output
    hex_data = ["00", "01", "FF", "FF", "80", "00"]

    expected_output = [256, -1, 128]

    # Call the function with the test case
    result = unwrap_twos_complement(hex_data)

    # Assert that the result matches the expected output
    assert result == expected_output, f"Expected {expected_output}, but got {result}"

def test_calculate_confidence_intervals(linear_regression_data):
    """
    Unit test for the calculate_confidence_intervals function.
    """
    from bme554 import calculate_confidence_intervals

    x = linear_regression_data['AIN0 Voltage (V)']
    y = linear_regression_data['Blink Rate (Hz)']

    # Call the function with the sample data
    slope_ci, intercept_ci = calculate_confidence_intervals(x, y)

    assert len(slope_ci) == 2 and len(intercept_ci) == 2, "Each output should contain two values"
    assert abs(slope_ci[0] - 1.3385) < 1e-4, f"slope_ci[0] should be approximately 1.3385, got {slope_ci[0]}"
    assert abs(intercept_ci[0] - 0.9865) < 1e-4, f"intercept_ci[0] should be approximately 0.9865, got {intercept_ci[0]}"
