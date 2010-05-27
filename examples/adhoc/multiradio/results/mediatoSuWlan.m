function mediatoSuWlan = wlanAvg( numHost, numRadio, repetition, params, csv )
% averages the result CSV between repetitions by wlans
% mediatoSuWlan returns a [totRadio, params] matrix containing the average
% value of the csv statistic on wlans for the different values of param
foo = reformatCSV(numHost,numRadio,repetition, params, csv);
mediatoSuWlan = zeros(numHost*numRadio, params);
for i = uint32(1:params)
    mediatoSuWlan(:,i) = mean( foo(:, ((i-1)*repetition)+1:i*repetition), 2 ); 
end


end

