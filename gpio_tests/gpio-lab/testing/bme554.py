# Duke BME554L Python Module
# Mark Palmeri
#
# This module contains functions for testing purposes in BME554L.

from typing import List
import pandas as pd
import scipy as sp
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt

def read_hex_data(file_path: str) -> List[str]:
    """
    Reads hexadecimal data from a file and returns it as a list of strings.

    Parameters:
    file_path (Pathlib.Path): The path to the file containing hexadecimal data.

    Returns:
    List[str]: A list of hexadecimal strings.
    """
    from pathlib import Path
    with open(Path(file_path), 'r') as file:
        raw_data = file.read()

    # get rid of leading/trailing whitespace and split into lines
    data_lines = raw_data.strip().splitlines()

    hex_data = []

    for line in data_lines:
        # split on |, toss the ASCII columns, then split on whitespace
        hex_data_line = line.split('|')[0].split()
        # append to the hex data list
        hex_data.extend(hex_data_line[0:])

    return hex_data

def unwrap_twos_complement(hex_data: List[str]) -> List[str]:
    """
    Unwraps a 12-bit two's complement list from a 16-bit hexadecimal value list.

    Parameters:
    hex_data (List[str]): The hexadecimal data (12-bit, in 16-bit list) to unwrap.

    Returns:
    raw_voltage (List[str]): The unwrapped two's complement integer.
    """

    raw_voltage = []

    for i in range(0, len(hex_data) - 1, 2): # step by 2 to get every 2 bytes (16 bits) 

        # intepret every two bytes as a 16-bit integer
        raw_value = int(hex_data[i+1] + hex_data[i], 16)
        # mask to get the lower 12 bits (resolution of the ADC specified in the devicetree)
        raw_value &= 0x0FFF
        # interpret signed 12-bit integer (2s complement)
        if raw_value >= 0x800:  # 2048 is 1/2 of 4096 (12-bit int range)
            raw_value -= 0x1000 # subtract 4096
        raw_voltage.append(raw_value)

    return raw_voltage

def plot_with_fit(x: pd.Series, y: pd.Series, x_label: str, y_label: str, title: str):

    # Linear regression
    slope, intercept, r_value, p_value, std_err = sp.stats.linregress(x, y)
    line = slope * np.array(x) + intercept

    # Plotting
    plt.figure(figsize=(10, 6))
    ax = sns.lineplot(x=x, y=y, marker='o', label='Data')
    plt.plot(x, line, color='red', label=f'Linear fit: y = {slope:.2f}x + {intercept:.2f}, R² = {r_value**2:.5f}')
    # Adding error bars
    y_err = std_err * np.ones_like(y)
    ax.errorbar(x, y, yerr=y_err, fmt='o', color='blue', alpha=0.5)

    # Labeling the plot
    plt.xlabel(x_label)
    plt.ylabel(y_label)
    plt.title(title)
    plt.legend()
    plt.show()

def calculate_confidence_intervals(
    x: pd.Series | np.ndarray, y: pd.Series | np.ndarray
) -> tuple:
    """
    Calculate the 95% confidence intervals for the slope and intercept of a linear regression.

    Parameters:
    x (pd.Series or np.ndarray): The independent variable data.
    y (pd.Series or np.ndarray): The dependent variable data.

    Returns:
    tuple: A tuple containing the confidence intervals for the slope and intercept.
    """

    slope, intercept, r_value, p_value, std_err = sp.stats.linregress(x, y)
    n = len(x)
    t_value = sp.stats.t.ppf(0.975, df=n-2)
    slope_confidence_interval = (slope - t_value * std_err, slope + t_value * std_err)
    intercept_confidence_interval = (
        intercept - t_value * std_err * np.sqrt(1/n + np.mean(x)**2 / np.sum((x - np.mean(x))**2)),
        intercept + t_value * std_err * np.sqrt(1/n + np.mean(x)**2 / np.sum((x - np.mean(x))**2))
    )

    return slope_confidence_interval, intercept_confidence_interval