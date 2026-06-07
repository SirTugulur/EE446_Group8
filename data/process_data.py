import pandas as pd
import os

# ==========================================
# Configuration
# ==========================================
input_csv = 'all_throws.csv'  # Replace with your actual file name

# ==========================================
# Processing
# ==========================================
def split_throws(file_path):
    print(f"Loading data from {file_path}...")
    df = pd.read_csv(file_path)

    # 1. Drop the throw index column
    if 'throw_id' in df.columns:
        df = df.drop(columns=['throw_id'])
        print("Dropped 'throw_id' column.")

    # 2. Identify the start of a new throw
    # Creates a boolean mask (True) every time sample_index or time_ms is 0
    new_throw_mask = (df['sample_index'] == 0) | (df['time_ms'] == 0)

    # 3. Assign a unique ID to each throw block
    # cumulative sum increases by 1 every time it sees a True in the mask
    df['unique_throw_block'] = new_throw_mask.cumsum()

    # 4. Group the data and export
    grouped = df.groupby(['label', 'unique_throw_block'])

    count = 0
    for (label, block_id), group_data in grouped:
        # Create the folder for the throw type (e.g., 'Forehand') if it doesn't exist
        os.makedirs(label, exist_ok=True)

        # Remove the temporary tracking column before saving
        output_data = group_data.drop(columns=['unique_throw_block'])

        # Define the output path (e.g., Forehand/throw_1.csv)
        output_filename = os.path.join(label, f"throw_{block_id}.csv")

        # Save to CSV, keeping headers but dropping pandas row indices
        output_data.to_csv(output_filename, index=False)
        count += 1

    print(f"Successfully split into {count} individual throw CSVs!")

# Run the script
if __name__ == "__main__":
    split_throws(input_csv)
