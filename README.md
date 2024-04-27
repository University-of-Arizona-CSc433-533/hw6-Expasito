Author: Joshua Baus [joshuabaus@arizona.edu]


How to run the program:
Go into the 'src' directory and run make to build all depenencies. Then run ./test on the command line to have the program run.

Controls:
Use the 'P' key to save the current image as a ppm file saved in 'Assets'. 
Use the Left and Right arrows to adjust the size of the convolution kernel for the second attempt of HDR filtering
Use the Up and Down arrows to adjust the gamma value for the filtering.

Use the '1', '2', and '3' keys on the keyboard to choose which way to display the image:
'1' : regular image with no filtering
'2' : with just gamma correction
'3' : bilateral filtering applied


This project does use parallel threading to speed up the rendering since the 3rd version using bilateral filtering does
take some time. I have it set to 16 threads, though you can run with only 1.

Known issues:
At times, the bottom of the screen will lose pixels. It seems to be a math issue but I am unable to figure out why that happens.
It occurs regardless of bounds checking and gets worse as the size of the kernel gets larger. Not an error in program running, just
a visual error.

Choosing different images:
Go to Source.c in 'src' and select which file to open in the main function.
