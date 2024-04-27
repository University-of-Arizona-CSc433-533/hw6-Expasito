#include <stdio.h>
#include <stdlib.h>
#include "rgbe.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_platform.h>
#include <math.h>
#include <pthread.h>

// SDL rendering stuff
SDL_Window *window = NULL;
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_Event event;

// SDL interface to get key values
const uint8_t *KEYS;

// Just pointers for data
float *image;
float *luminance;
uint8_t *data;

// 0 is normal image, 1 is first attempt, 2 is second attempt
int choice = 0;

// Our gamma value for the filtering.
float gamma_ = 1;

// Size of our kernel at the start. For now, just start with a 5x5 kernel(2 pixels over each direction)
float size = 0;

/*
    Used for convolving luminance values and uses a non-linear weighting system.
    x and y are the coords of the pixel
    data is the luminance data
    width and height are the width and height of the image
    size is the number of pixels in each direction to convolve with
*/
float convl(int x, int y, float *data, int width, int height, int size) {

    float sum = 0;
    float div = 0;

    // This is the center pixel's luminace value
    float lum = data[y*width + x];

    for(int i = -size;i<=size;i++) {
        for(int j= -size;j<=size;j++){

            // Skip over ones that are out of bounds
            if(y+j < 0 || y+j > height || x+i < 0 || x +i > width){

            }else{
                // Else do convolution

                // Get the index
                int index = (y + j) * width + (x + i);

                // Use our weighting function to give a multiplier
                float multiplier = exp(-abs(log(lum) - log(data[index])));
                multiplier = 1.0f;
                
                // Add the log of our data times the multiplier
                sum += log(data[index]) * multiplier;
                
                // Have our divier add 1/ multiplier to keep the range correct
                div += 1/multiplier;

            }
        }
    }

    // return the 'mean' of this pixel's range
    return sum/div;
}

// Should the program keep running
int stayAlive = 1;

const int numThreads = 8;

int barrier[8];

// Arguments for our pthreads function
typedef struct Args {
    int start;
    int end;
    int width;
    int height;
    int i;
} Args;

void *process(void *args_) {
    Args *args = args_;

    while(stayAlive) {


        // Wait for the signal
        while(barrier[args->i] != 0 && stayAlive);

        // Few locals to use
        int cntr1 = 4*args->start, cntr2 = 3*args->start;

        // Calc the x and y values
        int x = args->start % args->width;
        int y = args->start / args->width;



        while(cntr1 < 4*args->end){

                float r,g,b;

                // Get the luminance value for this pixel
                float lum = luminance[cntr2/3];

                // This is what we multiply each value by
                float scale;

                // For each option, determine the scale to multiply each pixel by

                // Just the regular image, so just scale of 1.0
                if(choice == 0){
                    scale = 1.0;

                }
                // Gamma correction, so get the scale value and multiply
                else if(choice == 1){
                    scale = pow(lum, gamma_) / lum;

                }
                // Bilateral filtering method
                else{
                    // Get the low pass filter using our convolution method
                    float lowPass = convl(x,y,luminance,args->width,args->height,size);

                    // Then get highpass
                    float highPass = log(luminance[cntr2/3]) - lowPass;

                    // Get the log prime value by scaling our low pass value
                    float logLPrime = gamma_ * lowPass + highPass;

                    // Remove the log from it
                    float LPrime = exp(logLPrime);

                    // And this is our scale
                    scale = LPrime / luminance[cntr2/3];


                }


                /*
                    This code is run regardless. We want to scale the r,g,b value and keep in a range
                    And then save to the image 
                */
                r = scale * image[cntr2 + 0];
                g = scale * image[cntr2 + 1];
                b = scale * image[cntr2 + 2];

                // Clamp values
                r = (r * 255.0) > 255.0 ? 255 : r*255.0;
                g = (g * 255.0) > 255.0 ? 255 : g*255.0;
                b = (b * 255.0) > 255.0 ? 255 : b*255.0;

                // Save values
                data[cntr1 + 0] = (uint8_t)b;
                data[cntr1 + 1] = (uint8_t)g;
                data[cntr1 + 2] = (uint8_t)r;
                data[cntr1 + 3] = 255;

                // Increment by respective values
                cntr1 += 4;
                cntr2 += 3;

                // Wrap x accordingly
                x++;
                if(x >= args->width){
                    x= 0;
                    y++;
                }

            }

            // signal we got here
            barrier[args->i] = 1;

    }
    return NULL;



}

// Did we save the image on the last keypress
int saved = 0;


int main(int argc, char **argv) {

    // This is all loading image stuff from the RGBE file. For simplicity, 
    // the window will have the same width and height as the image loaded
    int width, height;
    // FILE *f = fopen("../Assets/Building.hdr","rb");
    // FILE *f = fopen("../Assets/Cathedral.hdr","rb");
    // FILE *f = fopen("../Assets/Desk.hdr","rb");
    // FILE *f = fopen("../Assets/Fog.hdr","rb");
    FILE *f = fopen("../Assets/Night.hdr","rb");
    RGBE_ReadHeader(f,&width,&height,NULL);
    image = (float *)malloc(sizeof(float)*3*width*height);
    RGBE_ReadPixels_RLE(f,image,width,height);
    fclose(f);


    printf("File loaded sucesfully\n");
    printf("Width: %d, Height: %d\n", width, height);

    // Allocate our buffer for luminance
    luminance = malloc(sizeof(float) * width * height);

    // Set up SDL. Short enough we don't need a whole function
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(width, height, 0, &window, &renderer);

    // Create our texture to render to
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,width,height);

    // Buffer for the texture
    data = malloc(sizeof(uint8_t) * width * height * 4);


    // Luminace does not change, so we can calculate it once
    // First we need to get the luminance value for every pixel
    for(int cntr=0;cntr < width * height * 3;){

        // Calculate the luminesence of our pixel
        float luminance_ = 1.0/61.0 * (20.0*image[cntr] + 40.0*image[cntr + 1] + image[cntr+2]);

        // And save to an array
        luminance[cntr/3] = luminance_;

        cntr += 3;
    }

    pthread_t threads[numThreads];
    Args args[numThreads];

    printf("%d %d = %d\n", width, height, width*height);

    for(int i=0;i<numThreads;i++){
        args[i].start = i * (width*height)/ numThreads;
        args[i].end = (i+1) * (width * height) / numThreads -1;
        args[i].width = width;
        args[i].height = height;
        args[i].i = i;
        printf("%d -- %d %d\n",i,args[i].start, args[i].end);
        pthread_create(&threads[i],NULL,process,&args[i]);
    }

    // exit(1);

    // Just so we know the program hasn't halted
    int iteration = 0;



    while(stayAlive){


        // Signal they can go
        for(int i=0;i<numThreads;i++){
            barrier[i] = 0;
        }

        // Now wait for each thread to finish
        for(int i=0;i<numThreads;i++){
            while(barrier[i] == 0);
        }




        // Now process inputs and write to screen

        KEYS = SDL_GetKeyboardState(NULL);
        SDL_PumpEvents();

        // Use escape to exit
        if(KEYS[SDL_SCANCODE_ESCAPE]){
            stayAlive = 0;
        }

        // Up and down to change gamma
        if(KEYS[SDL_SCANCODE_UP]){
            if(choice == 3) gamma_ -=.1; else gamma_ -=.005;
        }
        if(KEYS[SDL_SCANCODE_DOWN]){
            if(choice == 3) gamma_ +=.1; else gamma_ +=.005;
        }

        // Left and right to change size of kernel
        if(KEYS[SDL_SCANCODE_RIGHT]){
            size += .1;
        }
        if(KEYS[SDL_SCANCODE_LEFT]){
            size -= .1;
        }

        // Clamp size
        if(size < 0){
            size = 0;
        }

        // Update choice by which number they press
        if(KEYS[SDL_SCANCODE_1]){
            choice = 0;
        }
        if(KEYS[SDL_SCANCODE_2]){
            choice = 1;
        }
        if(KEYS[SDL_SCANCODE_3]){
            choice = 2;
        }

        if(KEYS[SDL_SCANCODE_P]){
            if(saved == 0){
                saved = 1;
                printf("Writing image\n");
                FILE *ppm = fopen("../Assets/adjusted.ppm","w");
                fprintf(ppm, "P3\n%d %d\n255\n",width,height);
                int c =0;
                for (int i = 0; i < height; i++) {
				    for (int j = 0; j < width; j++) {
					    fprintf(ppm, "%d %d %d\n", data[c + 2], data[c + 1], data[c + 0]);
					    c += 4;
			    	}
			    }

			// Close the image
			fclose(ppm);
            }else{
                saved = 0;
            }
        }

        printf("%d -- Gamma: %f, Size: %f, Option: %d\n", iteration++, gamma_, size, choice + 1);


        // Check for window event just in case if they use the 'X'
        while(SDL_PollEvent(&event)){
            if(event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE){
                stayAlive = 0;
            }
        }

        // And draw image
        SDL_UpdateTexture(texture, NULL, data, sizeof(uint8_t) * width * 4);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);


    }

    for(int i=0;i<numThreads;i++){
        pthread_join(threads[i], NULL);
    }

    

}