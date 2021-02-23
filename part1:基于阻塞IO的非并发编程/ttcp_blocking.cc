#include <examples/ace/ttcp/common.h>
#include <muduo/base/Timestamp.h>

#undef NDEBUG

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>

#include <netinet/in.h>
#include <arpa/inet.h>

static int acceptOrDie(uint16_t port)
{
  int listenfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(listenfd >= 0);

  int yes = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
  {
    perror("setsockopt");
    exit(1);
  }

  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(listenfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)))
  {
    perror("bind");
    exit(1);
  }

  if (listen(listenfd, 5))
  {
    perror("listen");
    exit(1);
  }

  struct sockaddr_in peer_addr;
  bzero(&peer_addr, sizeof(peer_addr));
  socklen_t addrlen = 0;
  int sockfd = ::accept(listenfd, reinterpret_cast<struct sockaddr*>(&peer_addr), &addrlen);
  if (sockfd < 0)
  {
    perror("accept");
    exit(1);
  }
  ::close(listenfd);
  return sockfd;
}

static int write_n(int sockfd, const void* buf, int length)
{
  int written = 0;
  while (written < length)
  {
    ssize_t nw = ::write(sockfd, static_cast<const char*>(buf) + written, length - written);
    if (nw > 0)
    {
      written += static_cast<int>(nw);
    }
    else if (nw == 0)
    {
      break;  // EOF
    }
    else if (errno != EINTR)
    {
      perror("write");
      break;
    }
  }
  return written;
}

static int read_n(int sockfd, void* buf, int length)
{
  int nread = 0;
  while (nread < length)
  {
    ssize_t nr = ::read(sockfd, static_cast<char*>(buf) + nread, length - nread);
    if (nr > 0)
    {
      nread += static_cast<int>(nr);
    }
    else if (nr == 0)
    {
      break;  // EOF
    }
    else if (errno != EINTR)
    {
      perror("read");
      break;
    }
  }
  return nread;
}

void transmit(const Options& opt)//客户端
{
  struct sockaddr_in addr = resolveOrDie(opt.host.c_str(), opt.port);
  printf("connecting to %s:%d\n", inet_ntoa(addr.sin_addr), opt.port);

  int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(sockfd >= 0);
  int ret = ::connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
  if (ret)
  {
    perror("connect");
    printf("Unable to connect %s\n", opt.host.c_str());
    ::close(sockfd);
    return;
  }

  printf("connected\n");
  muduo::Timestamp start(muduo::Timestamp::now());
  struct SessionMessage sessionMessage = { 0, 0 };
  sessionMessage.number = htonl(opt.number);
  sessionMessage.length = htonl(opt.length);
  if (write_n(sockfd, &sessionMessage, sizeof(sessionMessage)) != sizeof(sessionMessage))
  {
    perror("write SessionMessage");
    exit(1);
  }

  const int total_len = static_cast<int>(sizeof(int32_t) + opt.length);
  PayloadMessage* payload = static_cast<PayloadMessage*>(::malloc(total_len));
  assert(payload);
  payload->length = htonl(opt.length);
  for (int i = 0; i < opt.length; ++i)
  {
    payload->data[i] = "0123456789ABCDEF"[i % 16];
  }

  double total_mb = 1.0 * opt.length * opt.number / 1024 / 1024;
  printf("%.3f MiB in total\n", total_mb);

  for (int i = 0; i < opt.number; ++i)
  {
    int nw = write_n(sockfd, payload, total_len);
    assert(nw == total_len);

    int ack = 0;
    int nr = read_n(sockfd, &ack, sizeof(ack));//等待服务端发挥的ack
    assert(nr == sizeof(ack));
    ack = ntohl(ack);
    assert(ack == opt.length);
  }

  ::free(payload);
  ::close(sockfd);
  double elapsed = timeDifference(muduo::Timestamp::now(), start);
  printf("%.3f seconds\n%.3f MiB/s\n", elapsed, total_mb / elapsed);//输出带宽
}

void receive(const Options& opt)//服务端
{
  int sockfd = acceptOrDie(opt.port);//接受连接

  struct SessionMessage sessionMessage = { 0, 0 };//消息的个数，消息的长度
  if (read_n(sockfd, &sessionMessage, sizeof(sessionMessage)) != sizeof(sessionMessage))
  {
    perror("read SessionMessage");
    exit(1);
  }

  sessionMessage.number = ntohl(sessionMessage.number);
  sessionMessage.length = ntohl(sessionMessage.length);
  printf("receive number = %d\nreceive length = %d\n",
         sessionMessage.number, sessionMessage.length);
  const int total_len = static_cast<int>(sizeof(int32_t) + sessionMessage.length);//总长度
  PayloadMessage* payload = static_cast<PayloadMessage*>(::malloc(total_len));//获得内存存放消息
  assert(payload);

  for (int i = 0; i < sessionMessage.number; ++i)//读取number条消息
  {
    payload->length = 0;
    if (read_n(sockfd, &payload->length, sizeof(payload->length)) != sizeof(payload->length))
    {
      perror("read length");
      exit(1);
    }
    payload->length = ntohl(payload->length);
    assert(payload->length == sessionMessage.length);
    if (read_n(sockfd, payload->data, payload->length) != payload->length)
    {
      perror("read payload data");
      exit(1);
    }
    int32_t ack = htonl(payload->length);//发挥ack
    if (write_n(sockfd, &ack, sizeof(ack)) != sizeof(ack))
    {
      perror("write ack");
      exit(1);
    }
  }
  ::free(payload);//释放内存
  ::close(sockfd);
}
/*
为了把这个程序跑起来，费了好长时间
首先下载muduo文件包
然后安装它的一些前置条件boost等等
然后用build.sh编译生成include/bin文件夹
编译时还要手动加上链接静态库路径/名称   写Makefile

为了让ide不变红还要在setting.json里加上includePath
*/
int main(int argc, char* argv[]){
    printf("hello muduo!\n");
    Options client, server;
    server.host = "127.0.0.1";
    server.port = 54321;
    server.receive = true;

    client.host = "127.0.0.1";
    client.port = 54321;
    client.number = 50;
    client.length = 500;
    client.transmit = true;

    printf("%d\n", atoi(argv[1]));
    if(0 == atoi(argv[1])){
        printf("I am server.\n");
        receive(server);
    }else{
        printf("I am client.\n");
        transmit(client);
    }
/*
开启两个终端分别../bin/ttcp-blocking 0  ../bin/ttcp-blocking 1
输出如下：
    服务端:
    hello muduo!
    0
    I am server.
    receive number = 50
    receive length = 500
    客户端:
    hello muduo!
    1
    I am client.
    connecting to 127.0.0.1:54321
    connected
    0.024 MiB in total
    0.001 seconds
    18.117 MiB/s
*/
}