function output = mesh2plot( csv )
% Reformat the mesh simulation result vector to a mesh-like matrix
%   mesh2plot reformat the result vector to a mesh structure useful for 3d
%   plots
output = zeros(sqrt(length(csv)));
for i = 1:length(output)
    output(i,:) = csv( (i-1)*length(output)+1:i*length(output))';
end


end

