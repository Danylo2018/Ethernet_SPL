#include <stm32f4xx.h>
#include "cmsis_os2.h"

#include "lwip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/opt.h"
#include "lwip/tcpip.h"
#include "lwip/ip.h"
#include "lwip/api.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"

#define LCD_FRAME_BUFFER SDRAM_DEVICE_ADDR
#define MAIL_SIZE (uint32_t) 5

#define N 5

unsigned char *out_buffer;

typedef struct struct_sock_t {
  uint16_t y_pos;
  uint16_t port;
} struct_sock;
struct_sock sock01;

typedef struct struct_client_socket_t {
  struct sockaddr_in remotehost;
  socklen_t sockaddrsize;
  int accept_sock;
  uint16_t y_pos;
} struct_client_socket;
struct_client_socket client_socket01;

struct netif gnetif;
ip4_addr_t ipaddr;
ip4_addr_t netmask;
ip4_addr_t gw;
uint8_t IP_ADDRESS[4];
uint8_t NETMASK_ADDRESS[4];
uint8_t GATEWAY_ADDRESS[4];

void ETH_GPIO_Config(void) {
	/* Enable SYSCFG clock */
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

	/* Configure MCO1 (PA8) */
	/* This pin must be initialized as MCO, but not needed to be used */
	/* It looks like a bug in STM32F4 */
	/* Init alternate function for PA8 = MCO */
	TM_GPIO_Init(GPIOA, GPIO_PIN_8, TM_GPIO_Mode_AF, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High);
	
#ifdef ETHERNET_MCO_CLOCK
	/* Set PA8 output HSE value */
	RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
#endif

	/* MII Media interface selection */
	SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_MII);
	/* Check if user has defined it's own pins */
	if (!TM_ETHERNET_InitPinsCallback()) {
		/* MII */
		/* GPIOA                     REF_CLK      MDIO         RX_DV */
		TM_GPIO_InitAlternate(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOB                     PPS_PUT      TDX3 */
		TM_GPIO_InitAlternate(GPIOB, GPIO_PIN_5 | GPIO_PIN_8, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOC                     MDC          TDX2         TX_CLK       RXD0         RXD1 */
		TM_GPIO_InitAlternate(GPIOC, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOG                     TX_EN         TXD0          TXD1 */
		TM_GPIO_InitAlternate(GPIOG, GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOH                     CRS          COL          RDX2         RXD3 */
		TM_GPIO_InitAlternate(GPIOH, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOI                     RX_ER */
		TM_GPIO_InitAlternate(GPIOI, GPIO_PIN_10, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
	}
}


//---------------------------------------------------------------
void ETH_MACDMA_Config(void) {
	/* Enable ETHERNET clock  */
	RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACRXEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACTXEN;

	/* Reset ETHERNET on AHB Bus */
	ETH_DeInit();

	/* Software reset */
	ETH_SoftwareReset();

	/* Wait for software reset */
	while (ETH_GetSoftwareResetStatus() == SET);

	/* ETHERNET Configuration --------------------------------------------------*/
	/* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
	ETH_StructInit(&ETH_InitStructure);

	/* Fill ETH_InitStructure parametrs */
	/*------------------------   MAC   -----------------------------------*/
	ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
	ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
	ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
	ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
	ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
	ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
	ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
	ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
	ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
	ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

	/*------------------------   DMA   -----------------------------------*/  
	/* When we use the Checksum offload feature, we need to enable the Store and Forward mode: 
	the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum, 
	if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
	ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable; 
	ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
	ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;

	ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
	ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;
	ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
	ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
	ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
	ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

	/* Configure Ethernet */
	EthStatus = ETH_Init(&ETH_InitStructure, ETHERNET_PHY_ADDRESS);
}

//---------------------------------------------------------------
void LWIP_Init(void)
{
  /* IP addresses initialization */
  IP_ADDRESS[0] = 192;
  IP_ADDRESS[1] = 168;
  IP_ADDRESS[2] = 1;
  IP_ADDRESS[3] = 191;
  NETMASK_ADDRESS[0] = 255;
  NETMASK_ADDRESS[1] = 255;
  NETMASK_ADDRESS[2] = 255;
  NETMASK_ADDRESS[3] = 0;
  GATEWAY_ADDRESS[0] = 192;
  GATEWAY_ADDRESS[1] = 168;
  GATEWAY_ADDRESS[2] = 1;
  GATEWAY_ADDRESS[3] = 1;
  
  /* Initilialize the LwIP stack with RTOS */
  tcpip_init( NULL, NULL );

  /* IP addresses initialization without DHCP (IPv4) */
  IP4_ADDR(&ipaddr, IP_ADDRESS[0], IP_ADDRESS[1], IP_ADDRESS[2], IP_ADDRESS[3]);
  IP4_ADDR(&netmask, NETMASK_ADDRESS[0], NETMASK_ADDRESS[1] , NETMASK_ADDRESS[2], NETMASK_ADDRESS[3]);
  IP4_ADDR(&gw, GATEWAY_ADDRESS[0], GATEWAY_ADDRESS[1], GATEWAY_ADDRESS[2], GATEWAY_ADDRESS[3]);

  /* add the network interface (IPv4/IPv6) with RTOS */
  netif_add(&gnetif, &ipaddr, &netmask, &gw, NULL, &ethernetif_init, &tcpip_input);

  /* Registers the default network interface */
  netif_set_default(&gnetif);

  if (netif_is_link_up(&gnetif))
  {
    /* When the netif is fully configured this function must be called */
    netif_set_up(&gnetif);
  }
  else
  {
    /* When the netif link is down this function must be called */
    netif_set_down(&gnetif);
  }
}
//---------------------------------------------------------------
static void client_socket_thread(void *arg)
{
	int buflen = 150;
  int ret, accept_sock;
  struct sockaddr_in remotehost;
	
  socklen_t sockaddrsize;
  struct_client_socket *arg_client_socket;
  arg_client_socket = (struct_client_socket*) arg;
	
	remotehost = arg_client_socket->remotehost;
  sockaddrsize  = arg_client_socket->sockaddrsize;
  accept_sock = arg_client_socket->accept_sock;
	
	accept_sock = arg_client_socket->accept_sock;
  for(;;)
  {
    ret = recvfrom( accept_sock, out_buffer, buflen, 0, (struct sockaddr *)&remotehost, &sockaddrsize);
		// If message recieved
		if(ret > 0)
    {
      out_buffer[ret] = 0;
			
			// If client wants to close connection
			if(strcmp((char*)out_buffer, "-c") == 0)
      {
        close(accept_sock);
        osThreadTerminate(NULL);
      }
			
			// Answer to server
			sendto(accept_sock,out_buffer,strlen((char*)out_buffer),0,(struct sockaddr *)&remotehost, sockaddrsize);
    }
	}
}
//---------------------------------------------------------------
static void tcp_thread(void *arg)
{
	struct_sock *arg_sock;
  int sock, accept_sock;
  struct sockaddr_in address, remotehost;
  socklen_t sockaddrsize;
  arg_sock = (struct_sock*) arg;
	
  if ((sock = socket(AF_INET,SOCK_STREAM, 0)) >= 0)
  {
		address.sin_family = AF_INET;
    address.sin_port = htons(arg_sock->port);
    address.sin_addr.s_addr = INADDR_ANY;
		
		if (bind(sock, (struct sockaddr *)&address, sizeof (address)) ==  0)
    { 
			// Bind OK
			listen(sock, N);
			
			for(;;)
      {
        accept_sock = accept(sock, (struct sockaddr *)&remotehost, (socklen_t *)&sockaddrsize);
				
				if(accept_sock >= 0)
        {
          client_socket01.accept_sock = accept_sock;
          client_socket01.remotehost = remotehost;
          client_socket01.sockaddrsize = sockaddrsize;
          client_socket01.y_pos = arg_sock->y_pos;
          sys_thread_new("client_socket_thread", client_socket_thread, (void*)&client_socket01, DEFAULT_THREAD_STACKSIZE, osPriorityNormal );
        }
				else 
				{
					// Accept failed
					close(accept_sock);
				}
			}
    }
    else
    {
			// Bind failed
      close(sock);
      return;
    }
  }
}
//---------------------------------------------------------------
int main()
{	
	ETH_GPIO_Config();
	ETH_MACDMA_Config();
	LWIP_Init();
	
	sock01.port = 7;
	sock01.y_pos = 60;
	
	sys_thread_new("tcp_thread", tcp_thread, (void*)&sock01, DEFAULT_THREAD_STACKSIZE * 2, osPriorityNormal);

	for(;;)
  {
    osDelay(1);
  }
}
