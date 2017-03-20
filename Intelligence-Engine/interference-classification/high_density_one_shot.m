% ########################################
% author: Pierluigi Gallo
% University of Palermo / CNIT
% pierluigi.gallo@unipa.it
% +39 091 23860.274
%
% ########################################

clear all;
close all;
%% FLAGS
DEBUG = false;
RECORDING = false;
FILE_RECORDING=false;
USE_DEBUG_TOPOLOGY= false;
USE_NEW_TOPOLOGY=true;
USE_LBEB = true;
PSEUDO_TDMA=false;
SLOTS_TO_PLOT=intmax;

repetitions = 10;


%% MAC parameters
%simulation duration [s]
simulation_duration=10;        %10s
% all nodes transmit pkts of the same lenght
pktduration = 1e-3;  %1ms
SIFS = 10e-6;       % 10us
slot=20e-6;         %20us  --- lot time of the DCF
% DIFS = SIFS + (2 * Slot time)
ACK_time = (92 + 160)*1e-6; % long preamble + 20 bytes sent at 1Mbps
DIFS= SIFS + 2*slot;
pkt_slot_dur= round(pktduration / slot); % duration of pkt transmission [# di slot]
retry_limit = 7; %after 7 retries the frame is discarded and CW is reset to CWmin
cwmin=15;  % 2^3-1    15-1024
cwmax=1023; % 2^i-1

%% TOPOLOGY
% number of nodes randomly distributed
n = 30;            % 30
% cov_radius = 100;  % coverage radius for each node. Increasing it,
 radius_vect = [80,90,100,110,120,150,200,300,500];
% radius_vect = [80,100,120,150,200,300];
% radius_vect = [100 300];


%% predictors for BUSY-IDLE TRACES FOR MACHINE LEARNING ANALYSIS
node_to_be_analyzed = 2; % created the busy-idle trace for the node indicated
sim_num = length(radius_vect)*repetitions;
% in case sim_num =1 the following variables can be created as rows,
% therefore we need to reshape them before including in the table

max_busy = zeros(sim_num,1);
busy_50 = zeros(sim_num,1);
busy_90 = zeros(sim_num,1);
max_idle = zeros(sim_num,1);
idle_50 = zeros(sim_num,1);
idle_90 = zeros(sim_num,1);

normalized_success = zeros(sim_num,1);
radius=zeros(sim_num,1);
node_ident=zeros(sim_num,1);
tx_neigh_G    =0;
tx_neigh_G2   =0;
rx_neigh_G    =0;
rx_neigh_G2   =0;
succ_norm_all =0;
tx_norm_all   =0;
num_of_cliques_tx   =0;
nodes_in_cliques_tx =0;
num_of_cliques_rx   =0;
nodes_in_cliques_rx =0;
retry_stats=zeros(sim_num, retry_limit);
output = {'initialization'};



directoryname = sprintf('results-%s---n-%d',datestr(now,'yyyymmdd-HHMMSS'), n);
mkdir(directoryname);

gind=0;
for cov_radius = radius_vect
    
    % simple topology where all hears all
    if(USE_DEBUG_TOPOLOGY)
        %adjacencies=[0 1 1 1; 1 0 1 1; 1 1 0 1; 1 1 1 0]; % all nodes hear each other
        %     adjacencies=[0 1 1 ; 1 0 1 ; 1 1 0];
        adjacencies = [0 1 0 0; 1 0 1 0; 0 1 0 1; 0 0 1 0]; % AP --> STA -- AP --> STA
        %adjacencies=[0 1 1 0; 1 0 1 0; 1 1 0 1; 0 0 1 0];
        [n, ~]=size(adjacencies);
        %     src_dst_mat=transmission_matrix(adjacencies);
        src_dst_mat = [0 1 0 0; 0 0 0 0; 0 0 0 1; 0 0 0 0];  % AP --> STA -- AP --> STA
        %     src_dst_mat = [0 1 0 0; 1 0 0 0; 0 0 0 1; 0 0 1 0];  % AP <--> STA -- AP <--> STA
    else
        %adjacency matrix (diagonal values are set to zero)
        %     [adjacencies src_dst_mat]=topology(n);
        [adjacencies, src_dst_mat]=topology(n, USE_NEW_TOPOLOGY, cov_radius); % n. of nodes, new topology, coverage radius
        %         disp(adjacencies);
        %         disp(src_dst_mat);
    end
    
    for index = 1:repetitions
        
        
        fprintf('cov_radius=%d (%d/%d), repetition (%d/%d) (therefore step %d of %d) \n', ...
            cov_radius, find(radius_vect==cov_radius), length(radius_vect), ...
            index, repetitions, (find(radius_vect==cov_radius)-1) * repetitions+index,  length(radius_vect)*repetitions);
        seq=1;
        
        
        if PSEUDO_TDMA
            %% TDMA PARAMETERS
            guardtime = 0; % guardtime in slot
            TDMA_frame=tdma_slot_num * pktduration; % the lenght of the whole frame
            TDMA_slot = pktduration + guardtime; % there is a guardtime of a pkt
            synched_nodes =false(1,n); % nodes that are working in tdma rather than in random access
            synch_time=zeros(1,n); % the time instant when the node passed to tdma (beginning of the first successfull transmission)
            untill_next_tdma_slot=ones(1,n); % 0 means that it's time to transmit
        end
        
        %% update contention window
        %contention windows exponent (for each node)
        % we save the exponent instead of the cw value;  doing so  we need
        % to double the exponent for colliding nodes
        % (instead to use a for cycle to upgrade contention window array)
        %           N    _           _
        % 0 < CW < 2  * |  CW    + 1  | - 1
        %               |_   min     _|
        cont_wind_exp=zeros(1,n); %all nodes start from the minimum cw
        cont_wind = 2 .^ cont_wind_exp *(cwmin+1)-1;
        
        
        %% RECORDING
        if RECORDING
            k=1; % index for recorded transmission time
            times = zeros(1,3); % recorded times will be appended
            % rec_tx is a matrix where rows are nodes, and columns are time indexes
            rec_tx = zeros(n,3,'uint8'); % 3 will be extended by appending rows
            % (uint8 allows to save space)
        end
        
        if FILE_RECORDING
            filename=sprintf('simulation_%s.dat',datestr(now,'yyyymmddHHMMSS'));
            file=fopen(filename,'a+');
        end
        
        
        %% update node status
        listening_nodes=zeros(1,n);
        % transmitting nodes
        % 0     not tranmsitting and listening
        % +n    transmitting, needs n slots to finish transmitting the pkt
        transmitting_nodes=zeros(1,n);
        
        % TRANSMITTING NEIGHBORS
        % consider, for each node, which of its neighbors are transmitting
        % txing_neigh = adjacencies & transmission
        % txing_neigh are
        %neighbors AND transmitting
        txing_neigh = zeros(1,n);
        total_tx = zeros(1,n);
        
        % BACKOFF
        % vector with the backoff value for nodes
        backoff = zeros(1,n);
        
        % TRASMITTED PACKETS
        % number of transmitted pkts for each node
        successful_tx = zeros(1,n);
        
        % COLLISIONS
        % number of collision events (per node)
        %  +----------------+----------------+----------------+    +----------------+
        %  |collisions node1|collisions node2|collisions node3| ...|collisions noden|
        %  +----------------+----------------+----------------+    +----------------+
        collisions = zeros(1,n); % nodes whose transmission is affected by collision
        collisions_at_rx = zeros(1,n); % nodes that hear a collision
        collided_slot = zeros(1,n);    %reveal collision in a time slot
        collided_rx_pkt  = zeros(1,n); % nodes that are receiving a pkt that is /has been corrupted by a collision
        
        % how long consecutive time nodes feel the channel idle
        idle_time=zeros(1,n); % in seconds
        
        % received_slots: how many consecutive slots are successfully received by a
        % node
        received_slots=zeros(1,n);
        % recorded channel trace for all n nodes
        rec_channel = zeros(int64(simulation_duration/slot), n);
        % retransmission retries
        % indicates the number of retries after collisions while transmitting a packet.
        % it increments till retry_limit, then a fresh frame is going to be
        % transmitted
        retries = zeros (1,n);
        ret_value_prev=0; % for predictors
        
        fprintf('************start********************************\n');
        tic
        %% start simulation
        for t=0:slot:simulation_duration
            %%%%%%%%%%%%%%%%%%%%%%%%%%%
            %%%%%%%%%%%%%%%%%%%%%%%%%%%
            %
            % slot-based actions
            %%%%%%%%%%%%%%%%%%%%%%%%%%%
            
            %% 1) check what happened during the previous slot
            
            %% neighbors of transmitting nodes
            transmitting_nodes_mat = repmat(transmitting_nodes, n, 1);
            txing_neigh = adjacencies & transmitting_nodes_mat; %to avoid one for cycle replicate the transmission row
            
            %% channel computation
            % for each node says if its channel is free (its neigbors are silent)
            % or if it is busy (at least one of its neighors is transmitting)
            % the i-th node sees the channel(i);
            % >=1 if busy, 0 if idle
            % if channel >=2 means that more than one neighbor is transmitting. If
            % this node is receiving, this results in a collision
            channel=sum(txing_neigh,2)'; % channel a column given by the the sum of interferences per nodes
            if (DEBUG)
                fprintf('%.2d ', channel); fprintf(' --- '); fprintf('%.2d ',transmitting_nodes); fprintf('\n');
            end
            rec_channel(int64(t/slot)+1, : )= channel;
            %% collision inferring
            %%% WARNING! COLLISIONS ARE REVEALED AT RECEIVER, NOT AT THE TRANSMITTER
            % a collision occurs when an intended receiver cannot decode a packet
            % beacause of other concurrent transmissions in the medium
            % a successful tx is when there is a successful rx
            % all nodes that are not transmitting are in listening
            listening_nodes=~logical(transmitting_nodes); % listening_nodes are those whose not transmitting
            interfered_nodes = listening_nodes & channel>1; % interfered nodes are those whose channel is used by more than one node
            
            %% collision detection
            % perform collision detection for each slot at receivers
            % when the pkt transmission ends, check if is was collided
            % upgrade coll/succ stats for their srcs ( transmitting nodes)
            % a whole pkt is received if for a src/dst src is ending transmission
            % and dst successfully received PKTSIZE consecutive slots
            active_listening_nodes = logical(transmitting_nodes * src_dst_mat);  % (src->dst)
            
            % nodes that have the following:
            % whose srcs are transmitting,
            % that are not interfered (channel <2)
            % that are listening
            % it means they have received another slot
            %% successful slots (evaluated at receiving nodes)
            receiving = listening_nodes & (~interfered_nodes) & active_listening_nodes;
            received_slots(logical(receiving)) = received_slots(logical(receiving)) + 1;
            
            %% collided slots (evaluated at receiving nodes)
            coll_rx = listening_nodes & interfered_nodes & active_listening_nodes;
            received_slots(logical(coll_rx)) = 0;
            collided_rx_pkt(logical(coll_rx))=1;
            
            
            % check if nodes whose srcs have finished a pkt have experienced a
            % collision on that pkt
            tx_end_nodes = (transmitting_nodes==1); % nodes who completed a pkt transmission (with or without collision)
            
            
            %% pkt-based actions (keep state along slots)
            if(sum(tx_end_nodes)) % only if in this slot there are nodes that have finished pkt transmission
                
                nodes_whose_src_ended_transmissions = double(tx_end_nodes) * double(src_dst_mat); % (SRC -> DST)double are used only because matrix multiplication (rows x cols) is for double in matlab and not for logicals
                %% check collisions at receivers
                coll_rx = nodes_whose_src_ended_transmissions & collided_rx_pkt;                   % receivers that experienced a collision
                succ_rx = nodes_whose_src_ended_transmissions & (received_slots==pkt_slot_dur);   % receivers that experienced a successful tx
                received_slots(logical(nodes_whose_src_ended_transmissions))=0;  % reset received slots for nodes that we are computing
                collided_rx_pkt(logical(nodes_whose_src_ended_transmissions))=0;   % reset collision of pkt in any case (collision or success)
                
                
                %% collided transmitters
                coll_tx = double(coll_rx) * double(src_dst_mat');     % ( DST -> SRC )
                coll_tx = coll_tx & tx_end_nodes; % needed because dst-> src is not a one-to-one mapping
                collisions(coll_tx) = collisions(coll_tx)+1;
                collisions_at_rx(coll_rx) = collisions_at_rx(coll_rx)+1;
                
                %% successful transmitters
                succ_tx = double(succ_rx) * double(src_dst_mat');     % ( DST -> SRC )
                succ_tx = succ_tx & tx_end_nodes; % needed because dst-> src is not a one-to-one mapping
                successful_tx(succ_tx) = successful_tx (succ_tx)+1;
                
                % contention window increase (for colliding stations)
                if(sum(coll_tx)) %only if at least a pkt collision occurred
                    
                    cont_wind_exp(coll_tx)=cont_wind_exp(coll_tx)+1;
                    cont_wind(coll_tx)=2 .^ (cont_wind_exp(coll_tx))*(cwmin+1)-1; %computes contention windows for colliding nodes
                    cont_wind(cont_wind>cwmax)=cwmax; % limit the contention windows to cwmax
                    
                    
                    
                    % implement retry limits. When a pkt is sent 7 times
                    % with collisions, reset the cont_wind to cwmin
                    retries = retries + (coll_tx & (cont_wind==cwmax)); % increments the number of retries of colliding transmitters that have cwmax
                    
                    %                     ret_value = retries(node_to_be_analyzed); % this is only to have another predictor for machine learning
                    %
                    %                     if(ret_value==(ret_value_prev+1) && ret_value<=retry_limit)
                    %                         fprintf('%d',ret_value);
                    %                         retry_stats(gind,ret_value)=retry_stats(gind, ret_value)+1;
                    %                     end
                    %                     ret_value_prev=ret_value;
                    
                    if (DEBUG & sum(retries>retry_limit))
                        disp('pkt dropped, max retry limit exceeded\n');
                    end
                    
                    cont_wind_exp(retries>retry_limit)= 0;
                    cont_wind(retries>retry_limit)=cwmin;
                    
                    retries(retries>retry_limit)=0;
                end
                % contention window reset    (for not colliding stations)
                %upgrade cw for nodes that successfully ended a transmission during this time slot (if any)
                if(sum(succ_tx)) %only if at least a successful transmission occurred
                    cont_wind(succ_tx)= cwmin;
                    cont_wind_exp(succ_tx)= 0;
                    retries(succ_tx) = 0;   % reset number of retries of successful transmitters
                end
                
            end
            
            %% backoff extraction
            %extract backoff for all nodes (only those who are really conting bk will be used)
            tmpbk = round(cont_wind .* rand(1,n)); %values in [0,1] *. [0,cw(i)] gives in [0, cw(i)]
            % stations not counting backoff should extract
            % backoff (saturated sources)
            not_counting_bk = ~logical(backoff); %also transmitting stations extract a bk for next contention
            backoff(not_counting_bk)=tmpbk(not_counting_bk);
            
            if FILE_RECORDING
                fprintf(file,'t=%d\n', t);
                fprintf(file,'TX/RX    '); dlmwrite(filename, transmitting_nodes, '-append','delimiter', '\t');
                fprintf(file,'CH       '); dlmwrite(filename, channel, '-append','delimiter', '\t');
                fprintf(file,'BK       '); dlmwrite(filename, backoff, '-append','delimiter', '\t');
                fprintf(file,'SUCC TX  '); dlmwrite(filename, successful_tx, '-append','delimiter', '\t');
                fprintf(file,'COL      '); dlmwrite(filename, collisions, '-append','delimiter', '\t');
                fprintf(file,'CW       '); dlmwrite(filename, cont_wind, '-append','delimiter', '\t');
                fprintf(file,'IDLE     '); dlmwrite(filename, idle_time, '-append','delimiter', '\t');
            end
            if DEBUG
                fprintf('t=%d\n', t);
                fprintf('TX/RX    ');     disp(transmitting_nodes);
                fprintf('CH       ');     disp(channel);
                fprintf('BK       ');     disp(backoff);
                fprintf('SUCC TX  ');     disp(successful_tx);
                fprintf('COL      ');     disp(collisions);
                fprintf('RETRIES  ');     disp(retries);
                fprintf('CW       ');     disp(cont_wind);
            end
            %% 2) take tx decision on the current slot
            % nodes which transmit during the slot are closer to finish their
            % transmission
            transmitting_nodes = transmitting_nodes - logical(transmitting_nodes); % transmission & transmission is the transmitting mask (who is tranmitting)
            
            %% how long channel has been idle (it is idle for a DIFS?)
            idle_time = idle_time + slot*(channel==0);
            idle_time(logical(channel) | logical(transmitting_nodes))=0;  % reset idle counting if channel is busy
            
            %% backoff decrementation
            % stations which are:
            % (i)   counting bk and
            % (ii)  listening an idle channel (more than a DIFS)
            % (iii) not transmitting,
            % have to decrement the bk
            decrementing = (logical(backoff)) & (logical(idle_time>=DIFS)) & (~(logical(transmitting_nodes)));
            backoff(logical(decrementing))=backoff(logical(decrementing))-1;
            
            %% starting transmission (ra = random access)
            % continue bk procedure also for tdma nodes (even if we do not take
            % care about it )
            %stations that have ALL of the following:
            % (i) ended bk
            % (ii) have an idle channel
            % (iii) are not yet transmitting,
            % (iv)  not synched (not in tdma mode)
            % (v) have 1s in rows of src_dst_mat (are valid sources)
            % they begin the transmission
            node_starting_ra_tx = logical((~backoff) & (~channel) & (~logical(transmitting_nodes)) & (sum(src_dst_mat,2)'));
            
            total_tx( node_starting_ra_tx) = total_tx(node_starting_ra_tx)+1;
            
            if PSEUDO_TDMA
                node_starting_ra_tx = logical((~backoff) & (~channel) & (~logical(transmitting_nodes)) & (~synched_nodes) & (sum(src_dst_mat,2)'));  % nodes starting tx because in random access
                
                
                if RECORDING
                    
                    %                 if (PSEUDO_TDMA)
                    %                     figurename= sprintf('tdma_numnodes_%d_trial_%d.pdf', n, seq);
                    %                 else
                    %                     figurename= sprintf('racc_numnodes_%d_trial_%d.pdf', n, seq);
                    %                 end
                    
                    
                    %% figure: throughput and collisions
                    %                 figure(3);
                    %                 clf(3);
                    %                 hold on;
                    %                 data=zeros(n,2);
                    %                 data(:,1)=successful_tx;
                    %                 data(:,2)=collisions;
                    %                 data(:,3)=collisions_at_rx;
                    %                 bar(data);
                    %                 title('transmitted pkts');
                    %                 xlabel('node id');
                    %                 ylabel('# of pkts');
                    %                 hold off;
                    %                 set(gca,'XTick',[1:n])
                    %                 print(3,'-dpdf',strcat('ra_vs_tdma/tx_pkts_',figurename));
                end
                
                
                %% start transmission (both random access and tdma)
                transmitting_nodes(node_starting_ra_tx | node_starting_tdma_tx )=pkt_slot_dur;
            else
                %% start transmission (only random access)
                transmitting_nodes(node_starting_ra_tx)=pkt_slot_dur;
            end
            % pause(1);
            if RECORDING %recording transmission events
                k=k+1;
                rec_tx(:,k)=transmitting_nodes';
            end
        end
        
        toc
        %fprintf('----%d --------',tdma_slot_num);
        fprintf('simulated time = %d s\n', simulation_duration);
        fprintf('nodes= %d\n', n);
        fprintf('cwmin = %d, cwmax = %d\n', cwmin, cwmax);
        fprintf('USE_LBEB = %d\n', USE_LBEB);
        fprintf('pkt duration = %d s\n', pktduration);
        %fprintf('pkts successfully transmitted '); disp(successful_tx);
        fprintf('total pkt tx with success = %d \n', sum(successful_tx,2));
        %fprintf('collisions = '); disp(collisions);
        fprintf('total collisions =%d\n', sum(collisions));
        fprintf('*************************************************\n');
        
        if RECORDING
            %% figure: transmission diagram
            figure(3);
            clf(3);
            hold on;
            title('transmissions diagram');
            xlabel('time [s]');
            ylabel('node id');
            max_to_plot=min(SLOTS_TO_PLOT, length(rec_tx)); %plot first 1000 slots
            xmax = round(max_to_plot/1)*slot;
            axis([0 xmax 0 n+0.5]);
            for p=1:n
                tobeplot=find(rec_tx(p,1:max_to_plot));
                scatter(tobeplot*slot,p*ones(1,length(tobeplot)),'*');
            end
            hold off;
        end
        %% figure: throughput and collisions
        figure(4);
        clf(4);
        hold on;
        data(:,1)=successful_tx;
        data(:,2)=collisions;
        data(:,3)=collisions_at_rx;
        bar(data);
        title('transmitted pkts');
        xlabel('node id');
        ylabel('# of pkts');
        legend({'succ tx','coll in tx', 'coll in rx'});
        hold off;
        
        figname = sprintf('%s/fig-throughput-n-%d_r-%d_id-%d.eps',directoryname, n , cov_radius, node_to_be_analyzed);
        print(4,'-depsc',figname);
        
        pkts_succ(index, seq) = sum(successful_tx);
        pkts_coll(index, seq) = sum(collisions);
        jain(index, seq) = jain_index(successful_tx);
        
        seq=seq+1;
        %    end
        
        
        %% figure: success and collisions (with repeteations)
        % figure(5);
        % clf(5);
        % hold on;
        % set(gca,'Xtick',1:length(cwmaxvect),'XTickLabel',{'7','15','31','63','127','255','1023'})
        % for i=1:repetitions
        %     scatter(1:length(cwmaxvect), pkts_succ(i,:),'ro');
        %     scatter(1:length(cwmaxvect), pkts_coll(i,:),'b*');
        % end
        % %% figure jain index (with repetitions)
        % figure(6);
        % clf(6);
        % hold on;
        % set(gca,'Xtick',1:length(cwmaxvect),'XTickLabel',{'7','15','31','63','127','255','1023'})
        % for i=1:repetitions
        %     scatter(1:length(cwmaxvect), jain(i,:), 'b*');
        % end
        
        %% figure: received power (how many colliding transmitters). It does not care about channel model
        % % figure;
        % % xtime = 0:slot:simulation_duration;
        % % xtime = xtime*1e3; % in msec
        
        % %
        % % plot(xtime, rec_channel(:,node_to_be_analyzed));
        % % ylim([-1 2]);
        % % xlabel('simulation time [ms]');
        % % ylabel('normalized received power');
        
        %% analysis of busy-idle traces
        %        busy_trace=zeros(int64(simulation_duration/slot)+1, n);
        %        busy_edges=zeros(int64(simulation_duration/slot)+1, n);
        %        busy_rise_edges=zeros(int64(simulation_duration/slot)+1, n);
        %        busy_falling_edges=zeros(int64(simulation_duration/slot)+1, n);
        %        busy_durations=zeros(int64(simulation_duration/slot)+1, n);
        %        idle_durations=zeros(int64(simulation_duration/slot)+1, n);
        
        busy_trace = logical(rec_channel);
        busy_edges = busy_trace - [ zeros(1,n); busy_trace(1:end-1,:)];
        
        for node_to_be_analyzed=1:n
            
            %% busy trace
            %(it starts with rising edge)
            %             busy_edges(:,node_to_be_analyzed)
            
            busy_rise_edges{node_to_be_analyzed} = slot*1000*find(busy_edges(:,node_to_be_analyzed)==1); % in msec
            busy_falling_edges{node_to_be_analyzed} = slot*1000*find(busy_edges(:,node_to_be_analyzed)==-1);
            
            
            if(~isempty(busy_rise_edges{node_to_be_analyzed})) % check if empty because it can happen that a node has not transmitting neighbors
                % therefore its channel is never busy
                gind=gind+1;
                
                if ( (length(busy_rise_edges{node_to_be_analyzed})-length(busy_falling_edges{node_to_be_analyzed})) ==1)
                    busy_rise_edges{node_to_be_analyzed} = busy_rise_edges{node_to_be_analyzed}(1:end-1);
                    %                     fprintf('busy durations required a trim\n');
                    %                 elseif (length(busy_rise_edges{node_to_be_analyzed})-length(busy_falling_edges{node_to_be_analyzed})==0)
                    %                     fprintf('all is ok with busy edges\n');
                    %                 else
                    %                     error('error computing busy/idle intervals\n');
                end
                
                
                % busy_durations is a cell array. In the example with 4 nodes,
                % AP --- STA --- AP --- STA where AP transmit, only STAs
                % hear sth, therefore we have a busy_duration like this:
                % []    [7516x1 double]    []    [8499x1 double]
                % to see the list of busy_duration run busy_durations{2}
                busy_durations{node_to_be_analyzed} = busy_falling_edges{node_to_be_analyzed} - busy_rise_edges{node_to_be_analyzed};
                idle_durations{node_to_be_analyzed} = busy_rise_edges{node_to_be_analyzed}(2:end) - busy_falling_edges{node_to_be_analyzed}(1:end-1);
                
                %% precdictors
                %
                %     ^
                %     |
                %  cdf..............................
                %     '                  _.---------
                %   0.9 .............,.''
                %     |        ;''''' |
                %     '       /       |
                %     |      /        |
                %     |     :         |
                %   0.5''''i|         |
                %     '   ;'|         |
                %     |  /  |         |
                %     | /   |         |
                %     |;    |         |     busy_time
                %     -+----+---------+------------->
                %          busy_50     busy_90
                
                
                %                                    PREDICTORS
                %               +--------------------------------------------------------|
                %             ...                                                        '
                %             | node_ident    max_busy    busy_50    busy_90    radius      output
                %             |    __________    ________    _______    _______    ______    __________
                %             |
                %             |    2             7.24        1          2.04       500       'no_i'
                % OBSERVATIONS|    4                1        1             1       500       'notavail'
                %             |    2             7.94        1          2.04       500       'no_i'
                %             |    4                1        1             1       500       'notavail'
                %             ''
                node_ident(gind)=node_to_be_analyzed;
                radius(gind)=cov_radius;
                
                %% busy-time
                max_busy(gind) = max(busy_durations{node_to_be_analyzed});
                
                [busy_cdf, busy_cdfx]=ecdf(busy_durations{node_to_be_analyzed});
                pos = find(busy_cdf>0.5);
                busy_50(gind) =  busy_cdfx(pos(1));
                
                pos = find(busy_cdf>0.9);
                busy_90(gind) =  busy_cdfx(pos(1));
                
                %% idle-time
                max_idle(gind) = max(idle_durations{node_to_be_analyzed});
                
                [idle_cdf, idle_cdfx]=ecdf(idle_durations{node_to_be_analyzed});
                pos = find(idle_cdf>0.5);
                idle_50(gind) =  idle_cdfx(pos(1));
                
                pos = find(idle_cdf>0.9);
                idle_90(gind) =  idle_cdfx(pos(1));
                
            end
        end % cycle over nodes to be analyzed
        
        %% classification output
        % todo:
        invalid = '---';
        %         normalized_success = successful_tx ./ total_tx;
        
        
        output_rep = cell(n,1);
        %         output_rep(isnan(normalized_success))                               ={''};
        %         normalized_success(total_tx==0)=0;
        tx_norm = total_tx ./ max(total_tx);
        succ_norm = successful_tx ./ max(successful_tx);
        
        output_rep( tx_norm <= 0.2)                     ={'collision'};
        output_rep( tx_norm > 0.2  & succ_norm <= 0.5) ={'hidden'};
        output_rep( succ_norm > 0.5)  ={'no_i'};
        
        output = [output; output_rep];
        
        %% topological predictors (n. of neighbors that are transmitting in G and G^2)
        G=adjacencies+eye(n); % for this purpose the adjacency matrix
        % requires also the diagonal
        G2 = logical(G * G);
        
        tx_neigh_G_tmp  =  sum(G,2)-1; %  how many neighbors at 1 hop are transmitting
        tx_neigh_G2_tmp =  sum(G2-G,2); % how many neighbors at exactly 2 hops are transmitting
        rx_neigh_G_tmp = sum( repmat(sum(G),n,1).* src_dst_mat , 2); % given nodes, count the number of the neighbors at 1 hop of their receivers
        rx_neigh_G2_tmp = sum( repmat(sum(G2-G),n,1).* src_dst_mat , 2); % given nodes, count the number of the neighbors at 2 hops of their receivers
        
        
        tx_neigh_G = [ tx_neigh_G; tx_neigh_G_tmp];
        tx_neigh_G2= [ tx_neigh_G2; tx_neigh_G2_tmp];
        rx_neigh_G = [ rx_neigh_G; rx_neigh_G_tmp];
        rx_neigh_G2= [ rx_neigh_G2; rx_neigh_G2_tmp];
        
        succ_norm_all = [succ_norm_all; succ_norm'];
        tx_norm_all = [tx_norm_all; tx_norm'];
        
        
        % there is no need to consider the transmission matrix since we
        % consider all nodes are transmitting
        maximal_cliques = maximalCliques(adjacencies);
        % the number of cliques any node belongs to
        num_of_cliques_tx = [num_of_cliques_tx; sum(maximal_cliques,2)];
        % sum of the number of nodes in the belonging cliques
        nodes_in_cliques_tx = [nodes_in_cliques_tx; maximal_cliques * sum(maximal_cliques)'];
        num_of_cliques_rx = [num_of_cliques_rx; src_dst_mat * sum(maximal_cliques,2)];
        nodes_in_cliques_rx = [nodes_in_cliques_rx; src_dst_mat * maximal_cliques * sum(maximal_cliques)'];
        
        %% fig busy time (only one node of the whole topology is analyzed)
        %         figure(5);
        %         hold on;
        %         [busy_cdf, busy_cdfx]=ecdf(busy_durations{node_to_be_analyzed});
        %         stairs(busy_cdfx, busy_cdf);
        %         % stem(busy_cdfx(2:end),diff(busy_cdf));
        %         title('');
        %         ylabel('CDF busy time');
        %         xlabel('t [ms]');
        %         figname = sprintf('%s/fig-busy-time-n-%d_r-%d_id-%d.eps',directoryname, n , cov_radius, node_to_be_analyzed);
        %         print(5,'-depsc',figname);
        
        
    end % repetitions
end % cycle over coverage radii

tx_neigh_G = tx_neigh_G(2:end);% remove the first cell used for initialization
tx_neigh_G2 = tx_neigh_G2(2:end);% remove the first cell used for initialization
rx_neigh_G = rx_neigh_G(2:end);% remove the first cell used for initialization
rx_neigh_G2 = rx_neigh_G2(2:end);% remove the first cell used for initialization
output = output(2:end); % remove the first cell used for initialization
succ_norm_all = succ_norm_all(2:end);
tx_norm_all = tx_norm_all(2:end);
num_of_cliques_tx = num_of_cliques_tx(2:end);
nodes_in_cliques_tx = nodes_in_cliques_tx(2:end);
num_of_cliques_rx = num_of_cliques_rx(2:end);
nodes_in_cliques_rx = nodes_in_cliques_rx(2:end);

[rows, cols]=size(max_busy);
node_ident=reshape(node_ident,max(rows,cols),1);
max_busy=reshape(max_busy,max(rows,cols),1);
busy_50= reshape(busy_50,max(rows,cols),1);
busy_90= reshape(busy_90,max(rows,cols),1);
max_idle=reshape(max_idle,max(rows,cols),1);

idle_50= reshape(idle_50,max(rows,cols),1);
idle_90= reshape(idle_90,max(rows,cols),1);
tx_neigh_G= reshape(tx_neigh_G,max(rows,cols),1);
tx_neigh_G2= reshape(tx_neigh_G2,max(rows,cols),1);
radius= reshape(radius,max(rows,cols),1);
output= reshape(output,max(rows,cols),1);

data_table = table( ...
    node_ident, radius, ...      % discard, just for completeness 
    max_busy, busy_50, busy_90, max_idle, idle_50, idle_90, ... % channel features ...
    tx_neigh_G, tx_neigh_G2, rx_neigh_G, rx_neigh_G2, num_of_cliques_tx, nodes_in_cliques_tx, num_of_cliques_rx, nodes_in_cliques_rx, ...     % topological features
    succ_norm_all, tx_norm_all, output);
%retry_1, retry_2, retry_3, retry_4, retry_5, retry_6, retry_7, ...
filename = sprintf('%s/traces_output.csv', directoryname);
writetable(data_table,filename);

disp('now, please, import data_table using the classification learner app');

