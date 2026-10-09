// Указание подключенных цифровых входов.
int in1 = 6;

int in2 = 5;
int in3 = 4;
int in4 = 3;

// Указывание задержки, чем больше она будет,

// тем медленнее будет происходить вращение.
const int dl = 4;

void setup() {
pinMode(in1, OUTPUT);
pinMode(in2, OUTPUT);
pinMode(in3, OUTPUT);
pinMode(in4, OUTPUT);

}

void loop() {

// Подача напряжения согласно таблице.
digitalWrite( in1, LOW );

digitalWrite( in2, LOW );
digitalWrite( in3, HIGH );
digitalWrite( in4, HIGH );
delay(dl);

digitalWrite( in1, LOW );
digitalWrite( in2, LOW );
digitalWrite( in3, HIGH );
digitalWrite( in4, LOW );
delay(dl);

digitalWrite( in1, LOW );
digitalWrite( in2, HIGH );
digitalWrite( in3, HIGH );
digitalWrite( in4, LOW );
delay(dl);

digitalWrite( in1, HIGH );
digitalWrite( in2, HIGH );
digitalWrite( in3, LOW );
digitalWrite( in4, LOW );
delay(dl);

digitalWrite( in1, HIGH );
digitalWrite( in2, LOW );
digitalWrite( in3, LOW );
digitalWrite( in4, LOW );
delay(dl);

digitalWrite( in1, HIGH );
digitalWrite( in2, LOW );
digitalWrite( in3, LOW );
digitalWrite( in4, HIGH );
delay(dl);

digitalWrite( in1, LOW );
digitalWrite( in2, LOW );
digitalWrite( in3, LOW );
digitalWrite( in4, HIGH );
delay(dl);

}
