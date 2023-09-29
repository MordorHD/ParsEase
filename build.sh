sources=$(find src -name "*.c")
headers=$(find src -name "*.h")
objects=
do_linking=false
program=
do_debug=false

project_name=parsease
common_flags="-g"
# -Ibuild needs to be included so that gcc can find the .gch file
compiler_flags="$common_flags -Wall -Wextra -Ibuild -Isrc"
linker_flags="$common_flags"
linker_libs=""

set -o xtrace

mkdir -p build/tests || exit

for h in $headers
do
	if [ src/$project_name.h -nt build/$project_name.h.gch ] ||
		[ $h -nt build/$project_name.h.gch ]
	then
		gcc src/$project_name.h -o build/$project_name.h.gch || exit
		break
	fi
done

for s in $sources
do
	o="build/${s:4:-2}.o"
	h="${s::-2}.h"
	objects="$objects $o"
	if [ $s -nt $o ] || [ $h -nt $o ] || [ build/$project_name.h.gch -nt $o ]
	then
		gcc $compiler_flags -c $s -o $o || exit
		do_linking=true
	fi
done

if $do_linking || [ ! -f build/$project_name ]
then
	gcc $linker_flags $objects -o build/$project_name $linker_libs || exit
fi

while [ ! $# = 0 ]
do
	case $1 in
	-t)
		program=test
		shift
		[ $# = 0 ] && ( echo "-t is missing argument" && exit )
		[ tests/$1.c -nt build/tests/$1.o ] &&
			( gcc $compiler_flags -c tests/$1.c -o build/tests/$1.o || exit )
		exc_objects="${objects/'build/main.o'/} build/tests/$1.o"
		gcc $linker_flags $exc_objects -o build/test $linker_libs || exit
		shift
		break
		;;
	-x)
		program=$project_name
		shift
		break
		;;
	-g)
		[ ! -z "$program" ] && program=$project_name
		do_debug=true
		;;
	*)
		echo invalid argument "$1"
		exit
		;;
	esac
	shift
done

if $do_debug
then
	gdb ./build/$program
elif [ ! -z "$program" ]
then
	time_now=$(date "+%s %N")
	read start_seconds start_nanoseconds <<< "$time_now"
	./build/$program $@
	exit_code=$?
	time_now=$(date "+%s %N")
	read end_seconds end_nanoseconds <<< "$time_now"
	diff_seconds=$((10#$end_seconds - 10#$start_seconds))
	diff_nanoseconds=$((10#$end_nanoseconds - 10#$start_nanoseconds))
	if [ $diff_nanoseconds -lt 0 ]
	then
		diff_seconds=$((diff_seconds - 1))
		diff_nanoseconds=$((1000000000 + diff_nanoseconds))
	fi
	echo -e "exit code: \e[36m$exit_code\e[0m; elapsed time: \e[36m$diff_seconds.$diff_nanoseconds\e[0m seconds"
fi
