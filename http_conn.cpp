#include "http_conn.h"
/*定义HTTP响应的一些状态信息*/
const char*ok_200_title = "OK";
const char*error_400_title = "Bad Request";
const char*error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char*error_403_title = "Forbidden";
const char*error_403_form = "You do not have permission to get file from this server.\n";
const char*error_404_title = "Not Found";
const char*error_404_form = "The requested file was not found on this server.\n";
const char*error_500_title = "Internal Error";
const char*error_500_form = "There was an unusual problem serving the requested file.\n";
/*网站的根目录*/
const char*doc_root = "./var/www/html";

int http_conn::m_user_count = 0;
myEpoll* http_conn::m_epoll = NULL;
MysqlDB* http_conn::mysqlDB = NULL;

int http_conn::get_socketfd(){
	return m_sockfd;
}
void http_conn::close_conn(bool real_close) {
	if (real_close && (m_sockfd != -1)) {
		m_epoll->removefd(m_sockfd);
		m_sockfd = -1;
		m_user_count--;/*关闭一个连接时，将客户总量减1*/
	}
}

void http_conn::init(int sockfd, const sockaddr_in &addr) {
	m_sockfd = sockfd;
	m_address = addr;
	/*如下两行是为了避免TIME_WAIT状态，仅用于调试，实际使用时应该去掉*/
	int reuse = 1;
	setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
	m_epoll->addfd(sockfd, true);
	m_user_count++;
	init();
}

void http_conn::init()
{
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;
	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length = 0;
	m_host = 0;
	m_start_line = 0;
	m_checked_idx = 0;
	m_read_idx = 0;
	m_write_idx = 0;
	memset(m_read_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);
}

/*从状态机*/
LINE_STATUS http_conn::parse_line()
{
	char temp;
	for (; m_checked_idx<m_read_idx; ++m_checked_idx)
	{
		temp = m_read_buf[m_checked_idx];
		if (temp == '\r')
		{
			if ((m_checked_idx + 1) == m_read_idx)
			{
				return LINE_OPEN;
			}
			else if (m_read_buf[m_checked_idx + 1] == '\n')
			{
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
		else if (temp == '\n')
		{
			if ((m_checked_idx>1) && (m_read_buf[m_checked_idx - 1] == '\r'))
			{
				m_read_buf[m_checked_idx - 1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	return LINE_OPEN;
}
/*循环读取客户数据，直到无数据可读或者对方关闭连接*/
bool http_conn::read()
{
	if (m_read_idx >= READ_BUFFER_SIZE)
	{
		return false;
	}
	int bytes_read = 0;
	while (true)
	{
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		if (bytes_read == -1)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				break;
			}
			return false;
		}
		else if (bytes_read == 0)
		{
			return false;
		}
		m_read_idx += bytes_read;
	}
	return true;
}
/*解析HTTP请求行，获得请求方法、目标URL，以及HTTP版本号*/
HTTP_CODE http_conn::parse_request_line(char*text)
{
	m_url = strpbrk(text, " \t");
	if (!m_url)
	{
		return BAD_REQUEST;
	}
	*m_url++ = '\0';
	char*method = text;
	if (strcasecmp(method, "GET") == 0)
	{
		m_method = GET;
	}
	else if(strcasecmp(method,"POST")==0){
		m_method = POST;
	}
	else{
		return BAD_REQUEST;
	}
	m_url += strspn(m_url, " \t");
	m_version = strpbrk(m_url, " \t");
	if (!m_version)
	{
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	m_version += strspn(m_version, " \t");
	if (strcasecmp(m_version, "HTTP/1.1") != 0)
	{
		return BAD_REQUEST;
	}
	if (strncasecmp(m_url, "http://", 7) == 0)
	{
		m_url += 7;
		m_url = strchr(m_url, '/');
	}
	if (!m_url || m_url[0] != '/')
	{
		return BAD_REQUEST;
	}
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}

/*解析HTTP请求的一个头部信息*/
HTTP_CODE http_conn::parse_headers(char*text)
{
	/*遇到空行，表示头部字段解析完毕*/
	if (text[0] == '\0')
	{
		/*如果HTTP请求有消息体，则还需要读取m_content_length字节的消息体，状态机转移到CHECK_STATE_CONTENT状态*/
		if (m_content_length != 0)
		{
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		/*否则说明我们已经得到了一个完整的HTTP请求*/
		return GET_REQUEST;
	}
	/*处理Connection头部字段*/
	else if (strncasecmp(text, "Connection:", 11) == 0)
	{
		text += 11;
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0)
		{
			m_linger = true;
		}
	}
	/*处理Content-Length头部字段*/
	else if (strncasecmp(text, "Content-Length:", 15) == 0)
	{
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);
	}
	/*处理Host头部字段*/
	else if (strncasecmp(text, "Host:", 5) == 0)
	{
		text += 5;
		text += strspn(text, " \t");
		m_host = text;
	}
	else
	{
		//printf("oop!unknow header%s\n", text);
	}
	return NO_REQUEST;
}
/*我们没有真正解析HTTP请求的消息体，只是判断它是否被完整地读入了*/
HTTP_CODE http_conn::parse_content(char*text)
{
	if (m_read_idx >= (m_content_length + m_checked_idx))
	{
		init_student();
		text[m_content_length] = '\0';
		text+=strspn(text," \t");
		char* next,*end=text+m_content_length;
		while(text<end){
			printf("begin:%s\n",text);
			next=strpbrk(text,"&");
			printf("begin1:%s\n",text);
			if(next!=NULL) *next='\0';
			printf("begin2:%s\n",text);
			if(strncasecmp(text,"id=",3)==0){
				text+=3;
				m_stu.id=text;
				printf("1:%s\n",m_stu.id);
			}
			else if(strncasecmp(text,"name=",5)==0){
				text+=5;
				m_stu.name=text;
				printf("2:%s\n",m_stu.name);
			}
			else if(strncasecmp(text,"sex=",4)==0){
				text+=4;
				m_stu.sex=text;
				printf("3:%s\n",m_stu.sex);
			}
			else if(strncasecmp(text,"birthday=",9)==0){
				text+=9;
				m_stu.birthday=text;
				printf("4:%s\n",m_stu.birthday);
			}
			else{
				return BAD_REQUEST;
			}
			printf("begin3\n");
			if(next==NULL) return GET_REQUEST;
			text=next+1;
		}
		return GET_REQUEST;
	}
	return NO_REQUEST;
}
/*主状态机。*/
HTTP_CODE http_conn::process_read()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char*text = 0;
	while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
		|| ((line_status = parse_line()) == LINE_OK))
	{
		text = get_line();
		m_start_line = m_checked_idx;
		printf("got 1 http line:%s\n", text);
		switch (m_check_state)
		{
		case CHECK_STATE_REQUESTLINE:
		{
			ret = parse_request_line(text);
			if (ret == BAD_REQUEST)
			{
				return BAD_REQUEST;
			}
			break;
		}
		case CHECK_STATE_HEADER:
		{
			ret = parse_headers(text);
			if (ret == BAD_REQUEST)
			{
				return BAD_REQUEST;
			}
			else if (ret == GET_REQUEST)
			{
				if(m_method==GET)
					return do_request_get();
			}
			break;
		}
		case CHECK_STATE_CONTENT:
		{
			ret = parse_content(text);
			if (ret == GET_REQUEST)
			{
				if(m_method==POST) 
					return do_request_post();
			}
			line_status = LINE_OPEN;
			break;
		}
		default:
		{
			return INTERNAL_ERROR;
		}
		}
	}
	return NO_REQUEST;
}
/*当得到一个完整、正确的HTTP的GET请求时，我们就分析目标文件的属性。如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址
m_file_address处，并告诉调用者获取文件成功*/
HTTP_CODE http_conn::do_request_get()
{
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);
	if(strcasecmp(m_url,"/show?")==0){
		mysqlDB->write_table();
		strcpy(m_real_file+len,"/show.html");
	}
	else{
		strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
	}
	
	if (stat(m_real_file, &m_file_stat) < 0)
	{
		return NO_RESOURCE;
	}
	if (!(m_file_stat.st_mode & S_IROTH))
	{
		return FORBIDDEN_REQUEST;
	}
	if (S_ISDIR(m_file_stat.st_mode))
	{
		return BAD_REQUEST;
	}
	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	return FILE_REQUEST;
}

/*当得到一个完整、正确的HTTP的POST请求时，我们就分析目标文件的属性。如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其映射到内存地址
m_file_address处，并告诉调用者获取文件成功*/
HTTP_CODE http_conn::do_request_post(){
	if(strcasecmp(m_url,"/add")==0){
		mysqlDB->add(m_stu.id,m_stu.name,m_stu.sex,m_stu.birthday);
		mysqlDB->print();
	}
	else if(strcasecmp(m_url,"/delete")==0){
		mysqlDB->del(m_stu.id);
		mysqlDB->print();
	}
	else if(strcasecmp(m_url,"/update")==0){
		if(m_stu.id==NULL) return NO_REQUEST;
		if(strcasecmp(m_stu.name,""))
			mysqlDB->update(m_stu.id,"name",m_stu.name);
		if(strcasecmp(m_stu.birthday,""))
			mysqlDB->update(m_stu.id,"birthday",m_stu.birthday);
		if(strcasecmp(m_stu.sex,""))
			mysqlDB->update(m_stu.id,"sex",m_stu.sex);
		mysqlDB->print(); 
	}
	return GET_REQUEST;
}

/*对内存映射区执行munmap操作*/
void http_conn::unmap()
{
	if (m_file_address)
	{
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = 0;
	}
}
/*写HTTP响应*/
bool http_conn::write()
{
	int temp = 0;
	int bytes_have_send = 0;
	int bytes_to_send = m_write_idx;
	if (bytes_to_send == 0)
	{
		m_epoll->modfd(m_sockfd, EPOLLIN);
		init();
		return true;
	}
	while (1)
	{
		temp = writev(m_sockfd, m_iv, m_iv_count);
		if (temp <= -1)
		{
			/*如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件。虽然在此期间，服务器无法立即接收到同一客户的下一个请求，但这可以保证连接的完整性*/
			if (errno == EAGAIN)
			{
				m_epoll->modfd(m_sockfd, EPOLLOUT);
				return true;
			}
			unmap();
			return false;
		}
		bytes_to_send -= temp;
		bytes_have_send += temp;
		if (bytes_to_send <= bytes_have_send)
		{
			/*发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接*/
			unmap();
			if (m_linger)
			{
				init();
				m_epoll->modfd(m_sockfd, EPOLLIN);
				return true;
			}
			else
			{
				m_epoll->modfd(m_sockfd, EPOLLIN);
				return false;
			}
		}
	}
}
/*往写缓冲中写入待发送的数据*/
bool http_conn::add_response(const char*format, ...)
{
	if (m_write_idx>= WRITE_BUFFER_SIZE)
	{
		return false;
	}
	va_list arg_list;
	va_start(arg_list, format);
	int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
	if (len>= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
	{
		return false;
	}
	m_write_idx += len;
	va_end(arg_list);
	return true;
}
bool http_conn::add_status_line(int status, const char*title)
{
	return add_response("%s%d%s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len)
{
	add_content_length(content_len);
	add_linger();
	add_blank_line();
    return true;
}
bool http_conn::add_content_length(int content_len)
{
	return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_linger()
{
	return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
	return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char*content)
{
	return add_response("%s", content);
}
/*根据服务器处理HTTP请求的结果，决定返回给客户端的内容*/
bool http_conn::process_write(HTTP_CODE ret)
{
	switch (ret)
	{
	case INTERNAL_ERROR:
	{
		add_status_line(500, error_500_title);
		add_headers(strlen(error_500_form));
		if (!add_content(error_500_form))
		{
			return false;
		}
		break;
	}
	case BAD_REQUEST:
	{
		add_status_line(400, error_400_title);
		add_headers(strlen(error_400_form));
		if (!add_content(error_400_form))
		{
			return false;
		}
		break;
	}
	case NO_RESOURCE:
	{
		add_status_line(404, error_404_title);
		add_headers(strlen(error_404_form));
		if (!add_content(error_404_form))
		{
			return false;
		}
		break;
	}
	case FORBIDDEN_REQUEST:
	{
		add_status_line(403, error_403_title);
		add_headers(strlen(error_403_form));
		if (!add_content(error_403_form))
		{
			return false;
		}
		break;
	}
	case FILE_REQUEST:
	{
		add_status_line(200, ok_200_title);
		if (m_file_stat.st_size != 0)
		{
			add_headers(m_file_stat.st_size);
			m_iv[0].iov_base = m_write_buf;
			m_iv[0].iov_len = m_write_idx;
			m_iv[1].iov_base = m_file_address;
			m_iv[1].iov_len = m_file_stat.st_size;
			m_iv_count = 2;
			return true;
		}
		else
		{
			const char*ok_string = "＜html＞＜body＞＜/body＞＜/html＞";
			add_headers(strlen(ok_string));
			if (!add_content(ok_string))
			{
				return false;
			}
		}
	}
	case GET_REQUEST:
	{
		add_status_line(200, ok_200_title);
		add_headers(strlen("The operation is OK."));
		if (!add_content("The operation is OK."))
		{
			return true;
		}
		break;
	}
	default:
	{
		return false;
	}
	}
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	return true;
}
/*由线程池中的工作线程调用，这是处理HTTP请求的入口函数*/
void http_conn::process()
{
	HTTP_CODE read_ret = process_read();
	if (read_ret == NO_REQUEST)
	{
		m_epoll->modfd(m_sockfd, EPOLLIN);
		return;
	}
	bool write_ret = process_write(read_ret);
	if (!write_ret)
	{
		close_conn();
	}
	m_epoll->modfd(m_sockfd, EPOLLOUT);
}


void http_conn::init_student(){
	m_stu.id=NULL;
	m_stu.name=NULL;
	m_stu.sex=NULL;
	m_stu.birthday=NULL;
}