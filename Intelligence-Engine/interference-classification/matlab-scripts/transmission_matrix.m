function [ tx_matrix ] = transmission_matrix( adjacencies )
%UNTITLED Summary of this function goes here
%   input: the adjacences matrix
%   output: the transmission matrix
% the transmission matrix is a matrix where values are a subset of the
% adjacencies matrix. If a node has n neighbors, it will choose to transmit
% only to one of them.

num = 1; %number of dest for each node (default is 1)
[dim,~]=size(adjacencies);

tx_matrix= false(dim,dim);

for i=1:dim % for all the nodes
    neighs = find(adjacencies(i,:));
    if(length(neighs)>1)% if the node has more than a neighbor, (otherwise randsample works differently if a scalar is given) take one randomly
    neigh = randsample(neighs,num);
    tx_matrix (i,neigh)=true;
    else
        tx_matrix(i,:)=adjacencies(i,:);
    end
    
end

check = (tx_matrix & adjacencies);
if (check ~= tx_matrix)
   error('error in transmission matrix'); 
end
%tx_matrix


end
