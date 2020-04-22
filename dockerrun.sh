 docker run -it --rm --net=host -v /etc/charm:/etc/charm -v $HOME:/root/ --sysctl net.core.rmem_max=26214400 charming sh
 