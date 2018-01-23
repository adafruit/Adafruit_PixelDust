# Adafruit_Sand Library [![Build Status](https://travis-ci.org/adafruit/Adafruit_Sand.svg?branch=master)](https://travis-ci.org/adafruit/Adafruit_Sand)

Particle simulation for "LED sand."

This library handles the "physics engine" part of a sand/rain simulation. It does not actually render anything itself and needs to work in conjunction with a display library to handle graphics. The term "physics" is used loosely here...it's a relatively crude algorithm that's appealing to the eye but takes many shortcuts with collision detection, etc.
