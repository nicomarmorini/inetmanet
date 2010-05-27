function reformatted = reformatCSV(numHost, numRadio, repetition, params, csv)
% Reformat the CSV produced by scavetool
%   reformatCSV simply regroup the result csv by runs. It is called
%   by the averaging functions to mediate between repetitions

reformatted = zeros(numHost * numRadio, repetition * params);
for i = uint32(1: (numHost * numRadio * repetition * params))
    reformatted(mod(i - 1, numHost * numRadio) + 1, idivide(i - 1, numHost * numRadio) + 1) = csv(i);
end


end

