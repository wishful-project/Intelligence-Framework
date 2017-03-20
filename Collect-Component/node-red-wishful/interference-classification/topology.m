% n = number of mesh nodes to create
% adjacencies = adj matrix (withouth the diagonal) - it is a neigh matrix
% using the variable number of argument we can have different behaviours
function [adjacencies, tx_matrix] = topology(n, varargin)

if (nargin < 1)
    disp('**********************************************************************');
    disp('no enough input args. Please, use [adjacencies src_dst_mat]=topology(n, USE_NEW_TOPOLOGY, cov_radius);');
end
% fprintf('Topology details --- Number of arguments: %d\n',nargin);
% celldisp(varargin)
% varargin{1} - boolean - if the nodes in the topology have to be created
% or if we will use a previously created topology
if (nargin < 2)
    redistribute_nodes = true; % by default we create a new topology
else
    redistribute_nodes = varargin{1};
end

% varargin{2} - the coverage radius of each node (default is 100)
if (nargin < 3)
    %coverage radius [m] %150m
    covr = 300;  %100
else
    covr = varargin{2};
end


adjacencies=zeros(n,n);
% field size [m]
xm = 300;
ym = 300;


%TOPOLOGY AND INITIALIZATION
figure(1);
clf (1);
title('mesh topology');
xlabel('x [m]');
ylabel('y [m]');
hold on;
axis([0 xm 0 ym]);
axis equal;
% persistent NODE; %like a static variable

if redistribute_nodes
    % node creation - uniformly distributed over the area
    for i=1:1:n
        %node coordinates
        NODE(i).coord=[rand(1,1)*xm  rand(1,1)*ym]; %[x y]
        % node queue occupancy
        NODE(i).queue = 0;
    end
    save('NODE.mat','NODE');
    
else
     load('NODE.mat');
end

for i=1:1:n
    plot(NODE(i).coord(1),NODE(i).coord(2), 'o');
    s = sprintf('%d',i);
    text(NODE(i).coord(1)+4,NODE(i).coord(2)+4, s);
end

% visualization of the edges of the graph
% and adjacency matrix initialization
for i=1:1:n
    for j=1:1:n
        if ((i~=j) && (norm(NODE(i).coord-NODE(j).coord)< covr))
            plot([NODE(i).coord(1) NODE(j).coord(1)], ...
                [NODE(i).coord(2) NODE(j).coord(2)]);
            adjacencies(i,j)=1; % use only the lower triangular, matrix is symmetrical
        end
    end
end

[s,c]=graphconncomp(sparse(adjacencies),'Directed',false);

% check if the graph is connected
if s>1
    %    s
    %    c
    fprintf('the graph is not connected!!!\n');
    %   error('the graph is not connected!!!\n');
    %nodes= find(c==mode(c)); % get nodes that are in the biggest connected graph available (mode takes the max frequent value)
end

%visualization of the transmitting nodes (edges in red)
if (nargin < 2)  || (varargin{1}==true)

    tx_matrix = transmission_matrix(adjacencies);
    save('tx_matrix.mat','tx_matrix');
else
    load('tx_matrix.mat');
end

for i=1:n
    src = i;
    dst = find(tx_matrix(i,:));
    
    plot([NODE(src).coord(1) NODE(dst).coord(1)], ...
        [NODE(src).coord(2) NODE(dst).coord(2)], '--r', 'LineWidth',2);
    %  plot(NODE(src).coord(1),NODE(src).coord(2), 'O', 'MarkerFaceColor',[0 0 1] ,'LineWidth',4);
    
end
hold off;
figname = sprintf('fig-topology-n-%d_r-%d.eps',n , covr);
print(1,'-depsc',figname);
end