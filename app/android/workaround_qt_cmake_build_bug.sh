#!/bin/bash


if ! pwd | grep -Eq ".*build.*Android.*"
# if [[ $(pwd) =~ "*build*Android*" ]]
then
    echo -e "\033[1;31mIt seems we are not in an Android build dir. The workaround should be started only in a Android build dir, anything else could destroy your system (this safeguard can easily fail)!\033[0m"
    echo -e "\033[33mReason: The directory $(pwd) doesn't match the regexp.\033[0m"
    exit
fi

for required_file_name in ./CMakeCache.txt build.ninja
do
    if [ ! -f $required_file_name ]
    then
        echo -e "\033[1;31mIt seems we are not in an Android build dir. The workaround should be started only in an Android build dir, anything else could destroy your system (this safeguard can easily fail)!\033[0m"
        echo -e "\033[33mReason: $required_file_name does not exist in $(pwd)\033[0m"
        exit
    fi
done

for required_dir_name in app plain_renderer nucleus gl_engine sherpa CMakeFiles
do
    if [ ! -d $required_dir_name ]
    then
        echo -e "\033[1;31mIt seems we are not in an Android build dir. The workaround should be started only in an Android build dir, anything else could destroy your system (this safeguard can easily fail)!\033[0m\n"
        echo -e "\033[33mReason: The directory $required_dir_name does not exist in $(pwd)\033[0m"
        exit
    fi
done

echo -e "\033[1;32mDeleting all libc.so from the build directory as a workaround for QTBUG-111284 / QTBUG-113851 (UnsatisfiedLinkError on Android)\033[0m"
echo -e "\033[32mNote: In directory $(pwd)\033[0m"
find -iname "libc.so" -delete
