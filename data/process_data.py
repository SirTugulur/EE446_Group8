import pandas as pd
import glob
import os
from datetime import datetime

# ==========================================
# Configuration
# ==========================================
ROOT_DIRECTORY = '.'  # Set to the directory containing your loose CSVs

# ==========================================
# Step 1: Split Throws (Root Files Only)
# ==========================================
def process_all_root_csvs(directory):
    print("--- STEP 1: SPLITTING THROWS ---")

    # Get all CSV files ONLY in the root directory (ignoring subfolders)
    root_csvs = [
        os.path.join(directory, f) for f in os.listdir(directory)
        if f.endswith('.csv') and os.path.isfile(os.path.join(directory, f))
    ]

    if not root_csvs:
        print("No CSV files found in the root directory.")
        return []

    # Generate a single timestamp for this specific run (Format: YYYYMMDD_HHMMSS)
    run_timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    total_split = 0

    for file_path in root_csvs:
        print(f"\nProcessing {os.path.basename(file_path)}...")

        try:
            df = pd.read_csv(file_path)

            # Drop the throw index column
            if 'throw_id' in df.columns:
                df = df.drop(columns=['throw_id'])

            # Identify the start of a new throw
            new_throw_mask = (df['sample_index'] == 0) | (df['time_ms'] == 0)
            df['unique_throw_block'] = new_throw_mask.cumsum()

            # Group the data and export
            grouped = df.groupby(['label', 'unique_throw_block'])

            count = 0
            for (label, block_id), group_data in grouped:
                # Force the label to be strictly lowercase and strip whitespace
                clean_label = str(label).strip().lower()

                # Create the lowercase folder for the throw type
                os.makedirs(os.path.join(directory, clean_label), exist_ok=True)

                # Remove the temporary tracking column before saving
                output_data = group_data.drop(columns=['unique_throw_block'])

                # Format: label_timestamp_001.csv
                formatted_name = f"{clean_label}_{run_timestamp}_{block_id:03d}.csv"
                output_filename = os.path.join(directory, clean_label, formatted_name)

                # Save to CSV
                output_data.to_csv(output_filename, index=False)
                count += 1
                total_split += 1

            print(f"  -> Split into {count} individual throw CSVs.")

            # Delete the original root file after successful split
            os.remove(file_path)
            print(f"  -> Deleted original file: {os.path.basename(file_path)}")

        except Exception as e:
            print(f"Error processing {file_path}: {e}")

    print(f"\nSuccessfully split a total of {total_split} throws across all root files!\n")
    return root_csvs

# ==========================================
# Step 2: Clean Duplicates
# ==========================================
def clean_duplicate_timestamps(directory, exclude_files):
    print("--- STEP 2: CLEANING DUPLICATES ---")

    # Recursively find all CSV files (including subfolders)
    search_pattern = os.path.join(directory, '**', '*.csv')
    all_csv_files = glob.glob(search_pattern, recursive=True)

    # Convert exclude list to absolute paths for safe comparison
    exclude_paths = {os.path.abspath(f) for f in exclude_files}

    # Filter out the root files we just processed so we only scrub the split throws
    all_csv_files = [f for f in all_csv_files if os.path.abspath(f) not in exclude_paths]

    print(f"Found {len(all_csv_files)} split CSV files. Beginning scan...")

    files_modified = 0
    total_duplicates_removed = 0

    for file_path in all_csv_files:
        try:
            df = pd.read_csv(file_path)

            if 'time_ms' in df.columns and 'sample_index' in df.columns:
                original_row_count = len(df)

                # Drop rows where the 'time_ms' is identical to a previous row
                df = df.drop_duplicates(subset=['time_ms'], keep='first')

                new_row_count = len(df)
                duplicates_found = original_row_count - new_row_count

                # If duplicates were removed, fix the index and save
                if duplicates_found > 0:
                    df['sample_index'] = range(len(df))
                    df.to_csv(file_path, index=False)

                    print(f"Cleaned: {os.path.basename(file_path)} | Removed {duplicates_found} duplicate(s)")

                    files_modified += 1
                    total_duplicates_removed += duplicates_found

        except Exception as e:
            print(f"Error processing {file_path}: {e}")

    print(f"Scan complete! Modified {files_modified} files and removed {total_duplicates_removed} total duplicate rows.")

# ==========================================
# Main Execution
# ==========================================
if __name__ == "__main__":
    # 1. Process all loose CSVs in the root folder, delete them, and get a list of what was processed
    processed_root_files = process_all_root_csvs(ROOT_DIRECTORY)

    # 2. Clean duplicates in all subfolders, explicitly ignoring the loose root files
    clean_duplicate_timestamps(ROOT_DIRECTORY, exclude_files=processed_root_files)
