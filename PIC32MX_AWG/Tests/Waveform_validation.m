clear all, close all, clc

% Load wave files
sine_file = fopen('validate_sine.txt');
square_file = fopen('validate_square.txt');
triangle_file = fopen('validate_triangle.txt');
arbitrary_file = fopen('validate_arbitrary.txt');
triangle_duty_file = fopen('validate_triangle_duty.txt');
square_duty_file = fopen('validate_square_duty.txt');

% Load text into arrays
sine = textscan(sine_file,'%d');
sine=sine{1}';

square = textscan(square_file,'%d');
square=square{1}';

triangle = textscan(triangle_file,'%d');
triangle=triangle{1}';

arbitrary = textscan(arbitrary_file,'%d');
arbitrary=arbitrary{1}';

triangle_duty = textscan(triangle_duty_file,'%d');
triangle_duty=triangle_duty{1}';

square_duty = textscan(square_duty_file,'%d');
square_duty=square_duty{1}';

% Plotting 
subplot(321)
plot([sine sine])
title("Sine Wave Validation");
xlabel("Sample")
ylabel("Duty cycle 0-254")
grid on

subplot(322)
plot(square)
title("Square Wave Validation");
xlabel("Sample")
ylabel("Duty cycle 0-254")
grid on

subplot(323)
plot(triangle)
title("Triangle Wave Validation");
xlabel("Sample")
ylabel("Duty cycle 0-254")
grid on

subplot(324)
plot(arbitrary)
title("Arbitrary Wave Validation");
xlabel("Sample")
ylabel("Duty cycle 0-254")
grid on

subplot(325)
plot(triangle_duty)
title("Triangle Wave 100%Duty Validation");
xlabel("Sample")
ylabel("Duty cycle 0-254")
grid on

subplot(326)
plot(square_duty)
title("Square Wave 80%Duty Validation");
xlabel("Sample")
ylabel("Duty cycle 0-254")
grid on

