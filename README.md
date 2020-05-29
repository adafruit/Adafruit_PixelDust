# Adafruit_PixelDust Library [![Build Status](https://github.com/adafruit/Adafruit_PixelDust/workflows/Arduino%20Library%20CI/badge.svg)](https://github.com/adafruit/Adafruit_PixelDust/actions)[![Documentation](https://github.com/adafruit/ci-arduino/blob/master/assets/doxygen_badge.svg)](http://adafruit.github.io/Adafruit_PixelDust/html/index.html)

Particle simulation for "LED sand" (or dust, or snow or rain or whatever).

This library handles the "physics engine" part of a sand/rain simulation. It does not actually render anything itself and needs to work in conjunction with a display library to handle graphics. The term "physics" is used loosely here...it's a relatively crude algorithm that's appealing to the eye but takes many shortcuts with collision detection, etc.

# Using Your Own Image on the LED Matrix Sand Toy #

![](https://cdn-learn.adafruit.com/assets/assets/000/051/316/medium640/raspberry_pi_hero-star.jpg?1519697034)

## Logo Example ##
The examples for the Raspberry Pi demonstrate a few ways to display graphics on the LED Matrix. The final example displays the [Adafruit](https://www.adafruit.com) logo on a colored background. Continuing reading for help on displaying your own image.

## Windows 10 UWP Application ##
For a code based example or to use an application that will turn an image into source code see **[https://github.com/porrey/ledmatrixide](https://github.com/porrey/ledmatrixide)**. This example is a fully function Windows 10 UWP application that will load an image and build code to be loaded on the [LED Matrix Sand Toy](https://learn.adafruit.com/matrix-led-sand).

## The Demo Logo ##
The image in the example is a gray scale image expressed in an array of values specifying the level of white for each LED in the matrix. Since white, and subsequently grays, are expressed as equal values of red, green and blue, only one 8-bit (1-byte) value is needed per pixel. The image is a 40 by 40 pixel image and therefore the array is defined as **uint8_t[40][40]**. The array is defined in the [header file](https://github.com/adafruit/Adafruit_PixelDust/blob/master/raspberry_pi/logo.h) of the example. 

The code iterates the array by row and column and calculates the color to display by blending the image pixel over the background color.

> **Pixel Blending** There are a variety of ways that imaging software can blend pixels. The most basic way is the take the alpha value of the foreground color and add it to 1 - alpha of the background color. For example, take a foreground color of **a=120**, **r=10**, **g=130**, **b=50** and a background color of **r=120**, **b=110**, **g=105**. Noting that the color values are 0 to 255, the alpha decimal value is 120 / 255 or .47, the final color would be r = (.47 * 10) + ((1 - .47) * 120), g = (.47 * 130) + ((1 - .47) * 110), b = (.47 * 50) + ((1 - .47) * 105) or **r = 68**, **g = 119**, **b = 79**.

The result of the blended pixel is applied to the LED matrix via the library method `led_canvas_set_pixel()`. This method, shown below, takes a position specified by the **x** (horizontal position or column, 0 to width - 1) and **y** (vertical position or row, 0 to height - 1) coordinates and the color expressed in its **red**, **green** and **blue** components (0 to 255).

    led_canvas_set_pixel(x, y, r, g, b);

> **Rendering** The set pixel method is always called within the `while(running)` loop and writes to an off-screen canvas. The current canvas and the off-screen canvas are swapped out during the refresh of the screen. This technique is common and is called double-buffering.

## Applying the Obstacles ##
### Library Method Used ###
The Pixel Dust library allows pixels on the grid to be defined as obstacles. The 'dust' pixels (or grains as they are referred to from this point forward) are unable to occupy these potions and will collide and bounce off of them. These pixels are defined using the `setPixel()` method. This method takes a position in the form of **x** (horizontal position or column, 0 to width - 1) and **y** (vertical position or row, 0 to height - 1)as shown below.

    sand->setPixel(x, y);

When displaying an image on the matrix, it stands to reason that we do not want the grains to occupy the same space and we would like them to bounce off the image. Drawing the image is not enough, we need to identify each pixel occupied by the as an obstacle. Since we only need to call the `setPixel()` method when we want to define the obstacle we are either calling it or NOT calling indicating we have a binary outcome. Thus, we need only a bit to represent each obstacle. If the bit is **1** then call `setPixel()` for the corresponding position, if it is **0** don't call it.

### Obstacles in the Demo ###
The demo defines a 40-bit by 40-bit array (which translate to a 40-byte by 5-byte array) corresponding to each pixel in the logo. The code that reads this array is shown below.

    int x1 = (width  - LOGO_WIDTH ) / 2;
    int y1 = (height - LOGO_HEIGHT) / 2;
    
    for(y=0; y<LOGO_HEIGHT; y++)
    {
    	for(x=0; x<LOGO_WIDTH; x++)
    	{
    		uint8_t c = logo_mask[y][x / 8];
    
    		if(c & (0x80 >> (x & 7)))
    		{
    			sand->setPixel(x1+x, y1+y);
    		}
    	}
    }

[View the output](https://raw.githubusercontent.com/porrey/ledmatrixide/master/Files/loop-output.txt) of this loop as it is iterated to help better understand how this structure is used to mark the obstacles.

The example outlined further down will express an easier to follow (not necessarily better, or worse) method at the expense of a larger file and higher memory usage. It further demonstrates that there are multiple ways to define the image and mask in your code.

## Defining the Grains ##
When the library instance is created, the number of grains is specified. The usage of the library constructor is shown below.

    sand = new Adafruit_PixelDust(w, h, n, s, e, sort);

The parameters are defined as follows (taken from the library header file):

**w**:    *Simulation width in pixels (up to 127 on AVR, 32767 on other architectures).*

**h**:    *Simulation height in pixels (same).*

**n**:    *Number of sand grains (up to 255 on AVR, 65535 elsewhere).*

**s**:    *Accelerometer scaling (1-255). The accelerometer X, Y and Z values passed to the `iterate()` function will be multiplied by this value and then divided by 256, e.g. pass 1 to divide accelerometer input by 256, 128 to divide by 2.*

**e**:    *Particle elasticity (0-255) (optional, default is 128). This determines the sand grains' "bounce" -- higher numbers yield bouncier particles.*

**sort**: *If true, particles are sorted bottom-to-top when iterating. Sorting sometimes (not always) makes the physics less "Looney Tunes," as lower particles get out of the way of upper particles. It can be computationally expensive if there's lots of grains, and isn't good if you're coloring grains by index (because they're constantly reordering).*

Once the instance is created call `begin()`. An example of this method is shown below.

    if(!sand->begin()) 
    {
    	puts("PixelDust init failed");
    	return 2;
    }

After the number of grains has been defined, their initial position must be defined. This can be done using one of two methods.

The first method is `randomize()` and is called to tell the library to randomly place all of the pixels on the screen when the application begins. This method does not take any parameters.

    sand->randomize();

The second method is `setPosition()` and is used to specify the starting position of the grains.

    sand->setPosition(i, x, y);

**i**:	*Grain index (0 to grains-1).*

**x**:	*Horizontal (x) coordinate (0 to width - 1).*

**y**:	*Vertical (y) coordinate (0 to height - 1).*

The method returns True on success (grain placed), otherwise false (position already occupied).

## The Basics of the Logo Demo ##

The logo demo uses the techniques above to draw the logo, define the obstacles and run the simulation.

In the code leading up to the `while` loop is where all of the initialization is done. The grains are defined, their initial position is determined (either set or randomized) and the obstacles are defined. At this point nothing has been drawn on the LED matrix

Within the loop, the accelerometer is read, the Pixel Dust library is updated using the `iterate()` method, the matrix is cleared using the background color, the logo is drawn and then the grains are drawn.

The grains are drawn by first asking the library the current position of each grain (note the grains are constantly moving) and then using the `led_canvas_set_pixel()` method to set a specific pixel color on the LED matrix. The color of the grain can be fixed as in some of the examples, randomized through some formula or by retrieving the color from an array that can be mapped using the grain index or pixel position (or whatever creative method you can think of).

All pixel drawing, regardless of its purpose is done using the `led_canvas_set_pixel()` (previously described).

## Displaying a Custom Image ##
When considering your own image, you need to resolve three approaches to get it up and running. These are

1. Translation of an image into a color map you can use to call `led_canvas_set_pixel()`
2. A structure defining the mask or obstacles that can be used to call `setPixel()`
3. Optionally, if you want to specify initial grain position and/or color, a structure that can be used to call the methods `setPosition()` and `led_canvas_set_pixel()`.

### 1. Mapping an Image ###
This step involves two parts. 

First, take an image such as a JPEG or PNG and get its color information in a known format. This most likely involves using an image library in your preferred language. The UWP application demonstrates how to do this in C# and .NET.

Second, place the color information into a structure that can be used to make calls to `led_canvas_set_pixel()`. There are probably an unlimited number of ways to do this and are at your discretion as the developer.

As an example, suppose we have a 2 pixel by pixel image. Further, let's assume our image library allowed us to convert it to an array of colors we could easily understand.

> **Pixel Mapping** The UWP C# code linked above uses a library that gets a one-dimensional array where the bytes are ordered as BGRA. in this structure, every 4 bytes defines one pixel of the image starting in the upper left corner and proceeding one row at a time. A 64 x 64-pixel image would be defined in an array that is 64 * 64 * 4 bytes long or 16,384 bytes. The C# code used to translate a pixel color from the one-dimensional array is shown below.

    uint index = (row * (uint)width + column) * 4;
    
    byte b = decodedBytes[index + 0];
    byte g = decodedBytes[index + 1];
    byte r = decodedBytes[index + 2];
    byte a = decodedBytes[index + 3];
    
    return Task.FromResult(Color.FromArgb(a, r, g, b));

Continuing with our 2 x 2 image example, suppose we have translated it to the structure shown below. The colors in this structure are in ARGB format.

    #define IMAGE_WIDTH  2
    #define IMAGE_HEIGHT 2
    
    const uint32_t image_color[IMAGE_HEIGHT][IMAGE_WIDTH] =
    {
    	0xFFD28825, 0xFFBB9520, 0xFFCB9A22, 0xFFCE9722
    }

The code below could be used to draw this image to LED matrix.

    // Center the image on the LED matrix
    int x1 = (width  - IMAGE_WIDTH ) / 2;
    int y1 = (height - IMAGE_HEIGHT) / 2;
    
    for(y = 0; y < IMAGE_HEIGHT; y++) 
    {
    	for(x = 0; x < IMAGE_WIDTH; x++) 
    	{
    		uint color = image_color[y][x];
    		
    		// Break the color into its components
    		uint8_t a = (color >> 24);  // Not used here
    		uint8_t r = (color >> 16);
    		uint8_t g = (color >> 8);
    		uint8_t b = (color >> 0);

    		// Draw the image pixel.
    		led_canvas_set_pixel(canvas, x1 + x, y1 + y, r, g, b);
    	}
    }

### 2. Mapping the Obstacles ###
The obstacles should correspond to the pixels in the image meaning anywhere you drew an image pixel you should define an obstacle (although it is perfectly acceptable to allow grains to roll over the image if this is an effect you are going for). A simple structure for this could be as shown below.

    const uint8_t image_mask[IMAGE_HEIGHT][IMAGE_WIDTH] =
    {
    	1, 1, 1, 1
    }

The code to define the obstacles using this structure would be as shown below.

    int x1 = (width  - IMAGE_WIDTH ) / 2;
    int y1 = (height - IMAGE_HEIGHT) / 2;
    
    for(y = 0; y < IMAGE_HEIGHT; y++) 
    {
    	for(x = 0; x < IMAGE_WIDTH; x++) 
    	{
    		uint8_t maskBit = image_mask[y][x];
    		
    		if (maskBit == 1)
    		{
    			sand->setPixel(x1 + x, y1 + y);
    		}
    	}
    }

### 3. Specify the Obstacle Position and Color ###
Regardless if we specify the initial grain position or the color, we need to define the number of grains. A good way to do this is using a define statement in the header file.

    #define NUM_GRAINS 4

Next, we need a structure that defines the position and color for each grain. This structure is read as an array of three-byte structures containing the **x** coordinate, the **y** coordinate and the **color** in that order. 

    const uint32_t grains[NUM_GRAINS][3] =
    {
    	 0,  0, 0xFF3A8EF6,
    	 1,  0, 0xFF3A8EF6,
    	 2,  0, 0xFF3A8EF6,
    	 3,  0, 0xFF3A8EF6
    }

The code that uses this structure to initialize the position of the grains is shown below.

    for(i = 0; i < NUM_GRAINS; i++) 
    {
    	uint8_t x = grains[i][0];
    	uint8_t y = grains[i][1];
    	sand->setPosition(i, x, y);
    }

The code that uses this structure to draw the grains is shown below. Note this code should be contained within the `while(running)` loop.

    for(i = 0; i < NUM_GRAINS; i++) 
    {
    	// Get the position of the grain.
    	sand->getPosition(i, &x, &y);
    	
    	// Get the color of the grain.
    	uint color = grains[i][2];
    
    	// Break the color into its components
    	uint8_t r = (color >> 16);
    	uint8_t g = (color >> 8);
    	uint8_t b = (color >> 0);
    
    	// Draw the sand pixel.
    	led_canvas_set_pixel(canvas, x, y, r, b, g);
    }

The sort parameter must be set to false in the library constructor when using the code above.

Of course, all of the examples above are one way of using your own image. Feel free to explore other ways to define the structures and write the code to interpret those structures.

## More Information ##

Additional information about the Pixel Dust library can be found in the [header file](https://github.com/adafruit/Adafruit_PixelDust/blob/master/Adafruit_PixelDust.h) which has detailed information about usage.
