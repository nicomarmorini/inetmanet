function output = mediatoSuHost( numHost, numRadio, repetition, params, csv )
% averages the result CSV between repetitions by host
% mediatoSuHost returns a [numHost, params] matrix containing the average
% value of the csv statistic on host for the different values of param
foo = mediatoSuWlan( numHost, numRadio, repetition, params, csv );
output = zeros( numHost, params );
for j = 1:params
    for i = 1:numHost
        output(i,j) = sum(foo( (i-1)*numRadio+1:i*numRadio, j));
    end
end
    

end

