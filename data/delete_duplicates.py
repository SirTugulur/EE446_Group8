import pandas as pd
import glob
import os

# ==========================================
# Configuration
# ==========================================
# Set this to the folder containing all your categorized throw folders
# (e.g., the folder that holds the 'Forehand', 'Backhand', etc. subfolders)
root_folder = '.'

# ==========================================
# Processing
# ==========================================
def clean_duplicate_timestamps(directory):
    # 1. Recursively find all CSV files in the directory and all subdirectories
    search_pattern = os.path.join(directory, '**', '*.csv')
    all_csv_files = glob.glob(search_pattern, recursive=True)

    print(f"Found {len(all_csv_files)} CSV files. Beginning scan...\n")

    files_modified = 0
    total_duplicates_removed = 0

    # 2. Process each file one by one
    for file_path in all_csv_files:
        try:
            df = pd.read_csv(file_path)

            # Safety check: Ensure the file actually has the required columns
            if 'time_ms' in df.columns and 'sample_index' in df.columns:

                original_row_count = len(df)

                # 3. Drop rows where the 'time_ms' is identical to a previous row
                # keep='first' ensures the original row stays, and the clone is deleted
                df = df.drop_duplicates(subset=['time_ms'], keep='first')

                new_row_count = len(df)
                duplicates_found = original_row_count - new_row_count

                # 4. If duplicates were removed, fix the index and save
                if duplicates_found > 0:
                    # Overwrite the sample_index column with a clean, continuous sequence
                    df['sample_index'] = range(len(df))

                    # Overwrite the original CSV file with the cleaned data
                    df.to_csv(file_path, index=False)

                    print(f"Cleaned: {os.path.basename(file_path)} | Removed {duplicates_found} duplicate(s)")

                    files_modified += 1
                    total_duplicates_removed += duplicates_found

        except Exception as e:
            print(f"Error processing {file_path}: {e}")

    print(f"\nScan complete! Modified {files_modified} files and removed {total_duplicates_removed} total duplicate rows.")

# Run the script
if __name__ == "__main__":
    clean_duplicate_timestamps(root_folder)
