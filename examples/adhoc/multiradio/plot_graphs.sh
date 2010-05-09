set view 0,0
set xtics 150
set ytics 150
#set palette rgbformulae 22,13,-31
set dgrid3d 11, 11
set hidden3d
splot 'lin.3' u 1:2:3 with pm3d
pause -1
