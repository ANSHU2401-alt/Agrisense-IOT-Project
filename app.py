from flask import Flask, request, jsonify
import joblib
import numpy as np

app = Flask(__name__)

model = joblib.load("model.pkl")
scaler = joblib.load("scaler.pkl")

def preprocess(X):
    X = X.copy()
    for i in range(X.shape[1]):
        X[:, i] = X[:, i] + 0.15 * (X[:, i] ** 2)
    return X
@app.route('/')
def home():
    return "Server Running"

@app.route('/predict', methods=['POST'])
def predict():
    data = request.json

    sm = float(data['sm'])
    temp = float(data['temp'])
    hum = float(data['hum'])

    X = np.array([[sm, hum, temp]])
    X = preprocess(X)
    X = scaler.transform(X)

    prob = model.predict_proba(X)[0][1]

    if sm > 70:
        pred = 0
    elif sm < 30:
        pred = 1
    else:
        pred = int(prob > 0.5)

    print(f"SM:{sm}, HUM:{hum}, TEMP:{temp}, PROB:{prob}, FINAL:{pred}")

    return jsonify({
        "pump": pred,
        "probability": float(prob)
    })

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000)
