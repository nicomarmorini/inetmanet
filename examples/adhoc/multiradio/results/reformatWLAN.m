function reformatted = reformatWLAN( numHost, numRadio, csv )
%UNTITLED Summary of this function goes here
%   Detailed explanation goes here

reformatted = zeros(numHost, numRadio);
for i = 1:numHost
    reformatted(i,:) = csv( (i-1)*numRadio + 1: i*numRadio )';
end

end

