
TCP: SOCK_STREAM socket()
UDP: SOCK_DGRAM
raw : SOCK_RAW

           
     int
     socket(int domain, int type, int protocol);

DESCRIPTION
     socket() creates an endpoint for communication and returns a descriptor.

     The domain parameter specifies a communications domain within which
     communication will take place; this selects the protocol family which
     should be used.  These families are defined in the include file
     ⟨sys/socket.h⟩.  The currently understood formats are

           PF_LOCAL        Host-internal protocols, formerly called PF_UNIX,
           PF_UNIX         Host-internal protocols, deprecated, use PF_LOCAL,
           PF_INET         Internet version 4 protocols,
           PF_ROUTE        Internal Routing protocol,
           PF_KEY          Internal key-management function,
           PF_INET6        Internet version 6 protocols,
           PF_SYSTEM       System domain,
           PF_NDRV         Raw access to network device,
           PF_VSOCK        VM Sockets protocols

     The socket has the indicated type, which specifies the semantics of
     communication.  Currently defined types are:

           SOCK_STREAM
           SOCK_DGRAM
           SOCK_RAW           

protocol: see /etc/protocols           



Member	Type	Purpose	
			
AF_INET:	 struct sockaddr_in (IPv4)		
sin_family	sa_family_t	Address Family. Must be set to AF_INET.	
sin_port	in_port_t	Port Number. Must be in Network Byte Order (big-endian), set using htons().	
sin_addr	struct in_addr	IPv4 Address. A structure containing the 32-bit IP address, which must also be in Network Byte Order, set using inet_pton() or htonl().	
sin_zero	char[8]	Padding. A space reserved to make the structure size equal to struct sockaddr. It should be set to all zeros (often using memset).	
			
AF_INET6	struct sockaddr_in6 (IPv6)		
sin6_family	sa_family_t	Address Family. Must be set to AF_INET6.	
sin6_port	in_port_t	Port Number. Must be in Network Byte Order.	
sin6_flowinfo	uint32_t	IPv6 traffic flow information (usually set to 0).	
sin6_addr	struct in6_addr	IPv6 Address. A structure containing the 128-bit IP address.	
sin6_scope_id	uint32_t	Identifier for the scope of the address (e.g., interface index for link-local addresses).	
			
AF_UNIX		struct sockaddr_un (UNIX Domain)	
sun_family	sa_family_t	Address Family. Must be set to AF_UNIX.	
sun_path	char[]	Filesystem Path. The null-terminated pathname in the filesystem to which the socket is bound.	

FOR ANY FAMILY:  struct sockaddr_storage (Generic Storage)
