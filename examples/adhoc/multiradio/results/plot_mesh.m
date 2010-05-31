function plot_mesh ( input )
% bar3 with colored bars according to height
%   Detailed explanation goes here

h = bar3(input);
for i = 1:length(h)
    zdata = ones(6*length(h),4);
    k = 1;
    for j = 0:6:(6*length(h)-6)
        zdata(j+1:j+6,:) = input(k,i);
        k = k+1;
    end
    set(h(i),'Cdata',zdata)
end
colormap summer

end

