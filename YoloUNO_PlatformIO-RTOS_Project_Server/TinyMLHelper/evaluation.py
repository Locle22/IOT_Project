import pandas as pd
from sklearn.metrics import confusion_matrix, classification_report

INPUT_CLEANED_CSV = "model_run_result.csv"

def main():
    try:
        df = pd.read_csv(INPUT_CLEANED_CSV)
    except Exception as e:
        print(f"[Error] Not Found! {INPUT_CLEANED_CSV}. {e}")
        return

    # Get label
    y_true = df['real_label']
    y_pred = df['predicted_label']

    # Calculate accuracy
    accuracy = (y_pred == y_true).mean() * 100

    print("="*60)
    print("EVALUATION RESULTS")
    print("="*60)
    print(f"Total test samples : {len(df)} samples")
    print(f"ACCURACY              : {accuracy:.2f}%\n")

    print("--- CONFUSION MATRIX ---")
    print("Rows: Actual Class (0=Normal, 1=Nuisance, 2=Fire), Columns: Predicted Class")
    cm = confusion_matrix(y_true, y_pred)
    print(cm)

    print("\n--- DETAILED CLASSIFICATION REPORT ---")
    print(classification_report(y_true, y_pred, target_names=['Normal (0)', 'Nuisance (1)', 'Fire (2)']))

if __name__ == "__main__":
    main()

