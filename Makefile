all:
	$(CXX) -shared -fPIC --no-gnu-unique main.cpp -o window-shaders.so -g `pkg-config --cflags pixman-1 libdrm hyprland` -std=c++2b 
clean:
	rm ./window-shaders.so