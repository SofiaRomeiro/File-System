#!/bin/bash


tests=$(find tests -name "*.c")


export red="\033[31;1m"
export green="\033[32;1m"
export reset="\033[0m"

echo "Compiling..."
make &> /dev/null
echo "Done"

X=0

for i in $tests;do

    ((X += 1))

    name=$(echo $i | cut -f 1 -d'.')
    echo "-------------------------------------------------------------------------"

    if [ -f $name ]; then
        ./$name &> /dev/null
    else
        echo -e "${red}$name${reset} does not exist"
        continue
    fi

    if [ $? -eq 0 ];then
        printf '%b' "Test $name -- ${green}OK$reset\n"
    else
        printf '%b' "Test $name -- ${red}FAILED${reset}\n"
    fi
done

echo ""
echo "$X testes passing! :D"

make clean &> /dev/null