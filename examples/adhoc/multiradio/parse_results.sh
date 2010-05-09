
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

declare -a PARAMETERS;
declare -a RUNS;
declare -a tmp;
declare -a numcollpernode;
declare -a numsentpernode;
declare -a numretrypernode;
declare -a numnoretrypernode;

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


let counter=0;
let applications=0;

for par in ${PARAMETERS[@]}; do 
let totalrecv=0; 
let totalsent=0; 
  for i in `ls results/$prefix*$par*.sca`; do
   applications=`grep "NetworkMultiRadioIPv4.mobileHost\[0].udpApp" $i | grep "Total send" | wc -l;`
    let sum=0; 
    let sum2=0; 
    for j in `seq 0 $((NUMNODES-1))`; do 
      # per ogni nodo greppa il numero di pacchetti inviati dall'UDP
     for p in `seq 0 $(($applications-1))`; do  
	tmpsum=0;
	tmpsum=`grep "Total send" $i | grep "Host\[$j]" | grep "udpApp\[$p]"  | awk '{print $NF}'` ; 
        sum=$(($sum+tmpsum))
     done;
     sum2=$(($sum2+`grep "Total received" $i | grep "Host\[$j]" |grep "udpApp\[0]"  | awk '{print $NF}'`)); 
    done; 
   totalsent=$(($sum+$totalsent)); 
   totalrecv=$(($sum2+$totalrecv)); 
   done 

let totalsentbymac=0; 
let totalcollision=0; 
let totalretry=0; 
let totalsentnoretry=0; 
  for i in `ls results/$prefix*$par*.sca`; 
   do let sumcollision=0; let sumsentbymac=0; let sumretry=0; let sumnoretry=0;
    for j in `seq 0 $((NUMNODES-1))`; do
      for m in `seq 0 $((NUMINTERFACES -1))`; do
      # per ogni nodo, per ogni interfaccia greppa il numero di pacchetti
      # inviati, ricevuti, collisioni e retry
      sumsentbymac=$(($sumsentbymac+`grep "numSent " $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      numsentpernode[$j]=$sumsentbymac;
      sumcollision=$(($sumcollision+`grep "numCollision" $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      numcollpernode[$j]=$sumcollision;
      sumretry=$(($sumretry+`grep "numRetry" $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      numretrypernode[$j]=$sumretry;
      sumnoretry=$(($sumnoretry+`grep "numSentWithoutRetry" $i | grep "Host\[$j]" |  grep "wlan\[$m]" | awk '{print $NF}'`)); 
      numnoretrypernode[$j]=$sumnoretry;
      done; 
    done; 
   totalsentbymac=$(($sumsentbymac+$totalsentbymac)); 
   totalsentnoretry=$(($sumnoretry+$totalsentnoretry)); 
   totalretry=$(($sumretry+$totalretry)); 
   totalcollision=$(($sumcollision+$totalcollision)); 
   done 
##
#
#  echo "Totale ricevuti per par $par " $totalrecv;
# done;
ratio=`echo "scale=2; 100*$totalrecv/$totalsent" | bc`;
echo "rapporto Inviati/Ricevuti per  $par: $totalsent/$totalrecv, $ratio%";
echo "Inviati $totalsent";
echo "Ricevuti $totalrecv";
echo "Ratio $ratio";
echo "Collisioni $totalcollision";
echo "SentByMac $totalsentbymac";
echo "SentNoRetry $totalsentnoretry";
echo "Retries $totalretry";

done;

