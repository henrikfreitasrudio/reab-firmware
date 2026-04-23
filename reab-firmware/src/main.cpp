#include <Arduino.h>
#include <Wire.h>

#define MPU_ADDR 0x68

// Raw
int16_t AcX_raw, AcY_raw, AcZ_raw;

// Convertido
float Xg, Yg, Zg;
float X, Y, Z;

// Ângulos
float pitch, roll;
float angulo; // eixo escolhido
float angulo_rel = 0;

// Offset (calibração)
float offset = 0;
bool calibrado = false;

// Filtro simples (low-pass)
float angulo_filtrado = 0;
float alpha = 0.1; // 0.0 a 1.0 (menor = mais suave)

// Controle de repetição
bool subindo = false;
bool descendo = false;
int repeticoes = 0;

// Tempo
unsigned long lastPrint = 0;

void setup()
{
    Serial.begin(115200);
    Wire.begin();

    // Wake MPU6050
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x6B);
    Wire.write(0);
    Wire.endTransmission(true);

    Serial.println("Sistema iniciado.");
    Serial.println("Digite 'c' para calibrar (posição 0°).");
}

void loop()
{

    // =========================
    // 📥 COMANDO SERIAL
    // =========================
    if (Serial.available())
    {
        char cmd = Serial.read();

        if (cmd == 'c')
        {
            offset = angulo_filtrado;
            calibrado = true;
            repeticoes = 0;

            Serial.println("Calibrado! Nova referência = 0°");
        }
    }

    // =========================
    // 📡 LEITURA MPU6050
    // =========================
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6, true);

    AcX_raw = Wire.read() << 8 | Wire.read();
    AcY_raw = Wire.read() << 8 | Wire.read();
    AcZ_raw = Wire.read() << 8 | Wire.read();

    // =========================
    // 🔄 CONVERSÃO PARA "g"
    // =========================
    Xg = AcX_raw / 16384.0;
    Yg = AcY_raw / 16384.0;
    Zg = AcZ_raw / 16384.0;

    // =========================
    // 🔄 NORMALIZAÇÃO
    // =========================
    float norm = sqrt(Xg * Xg + Yg * Yg + Zg * Zg);
    if (norm == 0)
        return;

    X = Xg / norm;
    Y = Yg / norm;
    Z = Zg / norm;

    // =========================
    // 📐 ÂNGULOS
    // =========================
    pitch = atan2(X, sqrt(Y * Y + Z * Z)) * 180.0 / PI;
    roll = atan2(Y, sqrt(X * X + Z * Z)) * 180.0 / PI;

    // 👉 Escolha do eixo (provavelmente roll para elevação lateral)
    angulo = roll;

    // =========================
    // 🎯 FILTRO (LOW PASS)
    // =========================
    angulo_filtrado = alpha * angulo + (1 - alpha) * angulo_filtrado;

    // =========================
    // 📏 ÂNGULO RELATIVO
    // =========================
    if (calibrado)
    {
        angulo_rel = angulo_filtrado - offset;
        if (angulo_rel < 0)
            angulo_rel = 0; // evita negativo
    }

    // =========================
    // 🔁 DETECÇÃO DE REPETIÇÃO
    // =========================
    float LIMIAR_SUBIDA = 20;  // começou movimento
    float LIMIAR_PICO = 80;    // topo (quase 90°)
    float LIMIAR_DESCIDA = 15; // voltou

    if (calibrado)
    {
        // Começou a subir
        if (angulo_rel > LIMIAR_SUBIDA && !subindo)
        {
            subindo = true;
            descendo = false;
        }

        // Chegou no topo
        if (angulo_rel > LIMIAR_PICO && subindo)
        {
            descendo = true;
        }

        // Voltou (finalizou repetição)
        if (angulo_rel < LIMIAR_DESCIDA && descendo)
        {
            repeticoes++;
            subindo = false;
            descendo = false;

            Serial.print("Repeticao completa! Total: ");
            Serial.println(repeticoes);
        }
    }

    // =========================
    // 🎯 METAS
    // =========================
    if (calibrado)
    {
        if (angulo_rel >= 30 && angulo_rel < 60)
        {
            Serial.println("Meta 1 (30°)");
        }
        else if (angulo_rel >= 60 && angulo_rel < 90)
        {
            Serial.println("Meta 2 (60°)");
        }
        else if (angulo_rel >= 90)
        {
            Serial.println("Meta FINAL (90°)");
        }
    }

    // =========================
    // 📊 PRINT CONTROLADO
    // =========================
    if (millis() - lastPrint > 200)
    {
        lastPrint = millis();

        Serial.print("X[g]: ");
        Serial.print(Xg, 2);
        Serial.print(" Y[g]: ");
        Serial.print(Yg, 2);
        Serial.print(" Z[g]: ");
        Serial.print(Zg, 2);

        Serial.print(" | Angulo: ");
        Serial.print(angulo_filtrado, 2);

        if (calibrado)
        {
            Serial.print(" | Relativo: ");
            Serial.print(angulo_rel, 2);
        }

        Serial.print(" | Reps: ");
        Serial.println(repeticoes);
    }
}