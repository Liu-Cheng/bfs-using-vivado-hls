# !/bin/sh

benchmark=("youtube" "lj1" "pokec" "rmat-19-32" "rmat-21-32")
for i in "${benchmark[@]}"; 
do
	sed -i "/std::string gName = /c\std::string gName = \"$i\";" ./src/host.cpp
	compile
	run > tmp.txt
	grep 'depth cache hit rate' ./tmp.txt | tail -1 >> ciao_hit_rate.txt
done

