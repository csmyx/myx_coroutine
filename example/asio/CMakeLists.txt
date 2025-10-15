find_package(asio CONFIG REQUIRED)

file(GLOB_RECURSE asio_examples *.cpp)
foreach(example ${asio_examples})
    get_filename_component(target_name ${example} NAME_WE)

    add_executable(${target_name} ${example})
    target_link_libraries(${target_name} PRIVATE asio::asio)
endforeach()
