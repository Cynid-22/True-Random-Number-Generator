# How to Compile and Run NIST SP 800-22 (STS)

The folder you downloaded (`sts-2.1.2`) contains the **source code** (C language), not the executable. You need to compile it first. Since you have **MSYS2** installed (which I saw in your error logs), this is easy!

## Step 1: Compile the Tool

1.  Open your **MSYS2 UCRT64** (or MinGW64) terminal.
2.  Navigate to the folder you downloaded:
    ```bash
    cd /c/Users/[YOUR_USERNAME]/Downloads/sts-2.1.2/sts-2.1.2
    # (Adjust path if it's somewhere else)
    ```
3.  **Edit the makefile**:
    *   Open `makefile` in a text editor.
    *   Change line 1 from `CC = /usr/bin/gcc` to `CC = gcc`.
    *   Save and close.
4.  Run the make command:
    ```bash
    make
    ```
    *   (If it says "make: command not found", run `pacman -S make gcc` first).
5.  This should create an executable file named `assess.exe` in the same folder.

## Step 2: Prepare for Testing

The NIST tool is famously "picky" about where files are located.

1.  **Copy** the `assess.exe` you just built.
2.  **Paste** it into the `experiments` folder inside the NIST directory.
    *   *Why?* The tool often looks for `templates/` relative to where it runs, or sometimes expects to write directly to `experiments/`. Running it *inside* `experiments` usually works best, or you might need to copy the `templates` folder into `experiments` as well.
    *   **Best Practice**: Keep `assess.exe` in the root `sts-2.1.2` folder, but make sure the `data/` directory exists or the tool assumes standard paths.

## Step 3: Run the Test

1.  Move your exported file (`nist_test.bin`) into the `data` folder inside the NIST directory.
2.  Run the tool from your terminal **inside the experiments folder**:
    ```bash
    cd experiments
    ../assess.exe 1000000
    ```
    *(The number `1000000` tells it the stream length upfront).*

3.  **Answer the Prompts in this exact order**:
    *   **Generator Selection**: Enter `0` (Input File).
    *   **User Prescribed Input File**: Enter `data/nist_test.bin`.
    *   **Select Test**: Enter `1` (To apply all tests).
    *   **Parameter Adjustments**: Enter `0` (To use defaults).
    *   **How many bitstreams?**: Enter `100` (for a quick test) or `800` (to test the entire 100MB file).
    *   **Input File Format**: Enter `1` (Binary).

    *The test will now start running. It typically takes **1 to 5 minutes** for 100 streams, or about **10-15 minutes** for the full 800 streams.*

## Step 4: Check Results

1.  Open `experiments/AlgorithmTesting/finalAnalysisReport.txt`.
2.  Scroll to the bottom.
3.  Look for the summary table. A passing score is usually **> 96%** (e.g., 96/100 streams passed).
