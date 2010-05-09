#! /bin/sh
# gen_topology numnodes lato distance
echo "[General]" 
for i in `seq 0 $(($1-1))`; do 
 echo \*.mobileHost[$i].mobility.x = $(($3 * ($i % $2))) 
 echo \*.mobileHost[$i].mobility.y = $(($3 * ($i / $2))) 
done;
