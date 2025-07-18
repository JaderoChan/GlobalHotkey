current_dir=$(pwd)

cd ../../
cmake -B ./build/shared/debug -DCMAKE_BUILD_TYPE=Debug -DGBHK_BUILD_SHARED=ON -DGBHK_BUILD_EXAMPLE=OFF
make -C ./build/shared -j
cmake -B ./build/shared/release -DCMAKE_BUILD_TYPE=Release -DGBHK_BUILD_SHARED=ON -DGBHK_BUILD_EXAMPLE=OFF
make -C ./build/shared -j

cd $current_dir
