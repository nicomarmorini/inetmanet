
# questo script si aspetta di trovare nella cartella results una serie di
# file di nome: NOMESCENARIO_parPARAMETRO_RUN_.sca
# conta quanti sono i parametri e le run per parametro poi media i valori
# di alcuni scalari per ogni scenario su tutte le run

# usage ./parse_results.sh NOMESCENARIO NUMNODES NUMINTERFACES


prefix=$1;
prefix+='_';

let runs=0;
let index=0;
let NUMNODES=$2;
let NUMINTERFACES=$3;
let numzone=3;

declare -a PARAMETERS;
declare -a RUNS;
declare -a tmp;
declare -a totnumcollpernode;
declare -a totnumsentwrpernode;
declare -a totnumsentpernode;
declare -a totnumretrypernode;
declare -a totnumqueuedroppedpernode;
declare -a totnumcollperzone;
declare -a totnumsentwrperzone;
declare -a totnumsentperzone;
declare -a totnumretryperzone;
declare -a totnumqueuedroppedperzone;
declare -a posxpernode;
declare -a posypernode;

for k in `ls results/$prefix*_0_.sca`; do 
  OLD_IFS="$IFS";   # cambia il separatore di linea, per tokenizzare il
                    # nome del file e rimettilo come era
  IFS="_"
  tmp=($k);
  IFS=$OLD_IFS;
  PARAMETERS[$index]=${tmp[1]}; # questo è il vettore dei parametri
  ((index++));
done

for k in `ls results/$prefix*${PARAMETERS[0]}*.sca`; do 
  OLD_IFS="$IFS";
  IFS="_"
  tmp=($k);
  IFS=$OLD_IFS;
  RUNS[$index]=${tmp[2]}; # vettore delle run
  ((index++));
done


for par in ${PARAMETERS[@]}; do 

for j in `seq 0 $((NUMNODES-1))`; do
totnumsentpernode[$j]=0;
totnumsentwrpernode[$j]=0;
totnumretrypernode[$j]=0;
totnumcollpernode[$j]=0;
totnumqueuedroppedpernode[$j]=0;
done;

for z in `seq 0 $((numzone-1))`; do
totnumcollperzone[$z]=0;
totnumsentwrperzone[$z]=0;
totnumsentperzone[$z]=0;
totnumretryperzone[$z]=0;
totnumqueuedroppedperzone[$z]=0;
done;

	for i in `ls results/$prefix*$par*.sca`; do 
    		for j in `seq 0 $((NUMNODES-1))`; do
		let sumcollision=0; let sumsentbymac=0; let sumqueuedrop=0; let sumretry=0;
		let sumsentwr=0;
			for m in `seq 0 $((NUMINTERFACES -1))`; do
      			# per ogni nodo, per ogni interfaccia greppa il numero di pacchetti
      			# inviati, collisioni e queue dropped 
      			sumsentbymac=$(($sumsentbymac+`grep "numSent " $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      			sumsentwr=$(($sumsentwr+`grep "numSentWithoutRetry " $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      			sumretry=$(($sumretry+`grep "numRetry" $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      			sumcollision=$(($sumcollision+`grep "numCollision" $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      			sumqueuedrop=$(($sumqueuedrop+`grep "packets dropped by queue" $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      
      			done;
		totnumsentpernode[$j]=$((${totnumsentpernode[$j]}+$sumsentbymac));
		totnumcollpernode[$j]=$((${totnumcollpernode[$j]}+$sumcollision));
		totnumqueuedroppedpernode[$j]=$((${totnumqueuedroppedpernode[$j]}+$sumqueuedrop));
		totnumsentwrpernode[$j]=$((${totnumsentwrpernode[$j]}+$sumsentwr));
		totnumretrypernode[$j]=$((${totnumretrypernode[$j]}+$sumretry));
	
		posxpernode[$j]=$((`grep "Position X" $i | grep "Host\[$j]" | awk '{print $NF}'`));  
		posypernode[$j]=$((`grep "Position Y" $i | grep "Host\[$j]" | awk '{print $NF}'`));  
    		done;
	done;

	echo -e "Config Name: $prefix\t$par";
	echo -e "X Coord\tY Coord\tSent\tColl\tQDrop\tRetry\tSentWR";
	for j in `seq 0 $((NUMNODES-1))`; do
		meansent=`echo "scale=0;${totnumsentpernode[$j]}/${#RUNS[@]}" | bc`;
		meansentwr=`echo "scale=0;${totnumsentwrpernode[$j]}/${#RUNS[@]}" | bc`;
		meanretry=`echo "scale=0;${totnumretrypernode[$j]}/${#RUNS[@]}" | bc`;
		meancoll=`echo "scale=0;${totnumcollpernode[$j]}/${#RUNS[@]}" | bc`;
		meanqueuedrop=`echo "scale=0;${totnumqueuedroppedpernode[$j]}/${#RUNS[@]}" | bc`;
		echo -e "${posxpernode[$j]}\t${posypernode[$j]}\t${meansent}\t${meancoll}\t${meanqueuedrop}\t${meanretry}\t${meansentwr}" >> plots/$prefix$par.3;
		echo -e "${posxpernode[$j]}\t${posypernode[$j]}\t${meansent}\t${meancoll}\t${meanqueuedrop}\t${meanretry}\t${meansentwr}";
		case $j in
		0|3|12|15)
			totnumsentperzone[0]=$((${totnumsentperzone[0]}+$meansent));
			totnumcollperzone[0]=$((${totnumcollperzone[0]}+$meancoll));
			totnumqueuedroppedperzone[0]=$((${totnumqueuedroppedperzone[0]}+$meanqueuedrop));
			totnumsentwrperzone[0]=$((${totnumsentwrperzone[0]}+$meansentwr));
			totnumretryperzone[0]=$((${totnumretryperzone[0]}+$meanretry));
		;;
		1|2|4|7|8|11|13|14)
			totnumsentperzone[1]=$((${totnumsentperzone[1]}+$meansent));
			totnumcollperzone[1]=$((${totnumcollperzone[1]}+$meancoll));
			totnumqueuedroppedperzone[1]=$((${totnumqueuedroppedperzone[1]}+$meanqueuedrop));
			totnumsentwrperzone[1]=$((${totnumsentwrperzone[1]}+$meansentwr));
			totnumretryperzone[1]=$((${totnumretryperzone[1]}+$meanretry));
		;;
		5|6|9|10)
			totnumsentperzone[2]=$((${totnumsentperzone[2]}+$meansent));
			totnumcollperzone[2]=$((${totnumcollperzone[2]}+$meancoll));
			totnumqueuedroppedperzone[2]=$((${totnumqueuedroppedperzone[2]}+$meanqueuedrop));
			totnumsentwrperzone[2]=$((${totnumsentwrperzone[2]}+$meansentwr));
			totnumretryperzone[2]=$((${totnumretryperzone[2]}+$meanretry));
		;;
		esac
	done;

	for z in `seq 0 $((numzone-1))`; do
	if [ "$z" -eq 0 ] || [ "$z" -eq 2 ]  ; then
		div=4;
	else 	
		div=8;
	fi
	meansentz=`echo "scale=0;${totnumsentperzone[$z]}/$div" | bc`;
	meansentwrz=`echo "scale=0;${totnumsentwrperzone[$z]}/$div" | bc`;
	meanretryz=`echo "scale=0;${totnumretryperzone[$z]}/$div" | bc`;
	meancollz=`echo "scale=0;${totnumcollperzone[$z]}/$div" | bc`;
	meanqueuedropz=`echo "scale=0;${totnumqueuedroppedperzone[$z]}/$div" | bc`;
	echo -e "Zone $z\t\t${meansentz}\t${meancollz}\t${meanqueuedropz}\t${meanretryz}\t${meansentwrz}";
	done;

	echo "";
	
done;
