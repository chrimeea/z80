Ruby version, not finished. Uses rom file at path roms/hc90.rom
cd ruby
ruby run.rb

C version, fully working. Needs a rom file.
cd c
gcc main.c -Ofast -lGLEW -lGLU -lGL -lglut -pthread -lm -Wall
./a.out file.rom
