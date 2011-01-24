% Parameters --------------------------------------------------------------
precision=10^(-2);
loops=100;
pi=3.14159265358979323846;

out=fopen('output-opt.txt','w');

for ii=1:20
tot=0;
for jj=1:5

% Read graph --------------------------------------------------------------
S=textread(['data/network/lod-cloud-',num2str(ii),'-opt.net'],'%q');
n=str2num(cell2mat(S(4))); % get number of nodes
i=2*n+6; % offset for the arcs definition


% Read connections --------------------------------------------------------
N=zeros(n,n);
for k=1:n^2 
    a=cell2mat(S(i));
	if a(1) == '*'
        break
    end
	N(str2num(a),str2num(cell2mat(S(i+1))))=str2num(cell2mat(S(i+2)));
	i=i+3;
end


% Read profiles -----------------------------------------------------------
i=i+4;
profile=zeros(n,1);
nb_profiles=0;
for k=i:i+n-1
    profile(1+k-i)=str2num(cell2mat(S(k)));
    if profile(1+k-i) > nb_profiles
        nb_profiles=profile(1+k-i);
    end
end
profile(profile<1)=nb_profiles+1;           % Assign dummy class
profile(profile>nb_profiles)=nb_profiles+1; % Assign dummy class


% ?????????????? ----------------------------------------------------------
profile_segr=0;
for k=1:nb_profiles
    if sum(profile==k)>0
        profile_segr=profile_segr - ( sum(profile==k)/n )*log( sum(profile==k)/n );
    end
end



% ?????????????? ----------------------------------------------------------
undN=N+N';
undN=+(undN>0);
und=sum(undN)';
avg_conn=sum(und)/n


% Compute no-partition entropy --------------------------------------------
z=rand(n,1); 
oldz=zeros(n,1);
for kk=1:loops
    for k=1:10
        U=ones(n,1)*z';
        D=ones(n,n) + z*z';  
        z=und ./ (sum( ( U./D - diag(diag(U./D)) )' ) )';
        z=max(z,10^(-15));
    end
    if max(abs(z-oldz))<precision
        break
    end
    oldz=z;
end
M=log( ones(n,n)+z*z' );
M2=(z*z')./( ( ones(n,n)+z*z' ).^2 );
alpha=sum(M2);
S=(1/n)*( - sum(log(z).*und) + sum(sum( triu(M,1) )) - ( sum(log(2*pi*alpha)) )/2 );
np_result=S


% Compute profiles entropy ------------------------------------------------
A=zeros(nb_profiles,nb_profiles);
for i=1:nb_profiles
    for j=i:nb_profiles
        A(i,j)=sum(sum((undN(profile==i,profile==j)) ));
        A(j,i)=A(i,j);
    end
    A(i,i)=A(i,i)/2;
end

z=rand(n,1);   
W=rand(nb_profiles,nb_profiles);
W=(W+W')/2;
oldW=zeros(nb_profiles,nb_profiles);
oldz=zeros(n,1);
for kk=1:loops
    bigW=W(profile,profile);    
    U=ones(n,1)*z' .* bigW;
    D=ones(n,n) + ( z*z'.* bigW); 
    z=und ./ (sum( ( U./D - diag(diag(U./D)) )' ) )';
    z=max(z,10^(-15));
        
    d=zeros(nb_profiles,nb_profiles);
    bigW=W(profile,profile);
    for i=1:nb_profiles
        for j=1:nb_profiles
            a=(profile==i)+0;
            b=(profile==j)+0;
            M=(a*b') .* (z*z') ./ ( ones(n,n) + ((z*z') .* bigW)) ;
            d(i,j)=( sum(sum( (M)-diag(diag(M)) )) );
            if (i==j)
                d(i,i)=d(i,i)/2;
            end
            if (d(i,j)*A(i,j))>0.0
                W(i,j)=A(i,j)/(d(i,j));
                W(i,j)=max(W(i,j),10^(-5));
                W(i,j)=min(W(i,j),10^5);
            else
                W(i,j)=0;
            end
        end
    end
            
    %max(max(abs(W-oldW)))
    %max(abs(z-oldz))
    if max(max(abs(W-oldW)))<precision && max(abs(z-oldz))<precision
        break
    end
	oldW=W;
    oldz=z;
end

M=log( ones(n,n) + ( (z*z').*W(profile,profile) ) ) ;
W=max( W, 10^(-15) );
M2=( (z*z').*W(profile,profile) )./( ( ones(n,n)+( (z*z').*W(profile,profile) ) ).^2 );
alpha=sum(M2);
Q=0;
for i=1:nb_profiles
	for j=1:i
        q=sum(sum((M2(profile==i,profile==j)) ));
        if q > 10^(-10)
            Q=Q + log(2*pi*q);
        end
	end
end

Sc=(1/n)*( - sum(log(z).*und) - sum(sum(  triu(A.*  log(W) ,0 )  ))  + sum(sum( triu( M,1) )) - ( sum(log(2*pi*alpha)) )/2  - Q/2 );
profile_result=Sc

[profile_segr np_result-profile_result]
tot=tot+np_result-profile_result;
end

tot=tot/5;

fprintf(out, '%d %8.4f\n', ii, tot);

end
fclose(out);
