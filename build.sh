#!/bin/bash

gcc -Wall -static -D_TEST_DISP_ utils.c texture.c display.c -lpthread -lgdi32

