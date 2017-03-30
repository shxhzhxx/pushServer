# pushServer
push server for web server (php etc) to push message to client


使用方法：

修改push.h的配置信息：

```c++
#define SERVER_IP_ADDRESS "192.168.2.109"     					//推送服务器的ip
#define BIND_SERVER_PORT "3889"									//开放给客户端绑定推送服务器的端口
#define PUSH_SERVER_PORT "3899"									//开放给服务器端的端口
#define TIME_OUT 5000
#define CALLBACK_TIME_OUT 10000
#define MAX_MESSAGE_SIZE 512									//推送信息的最大长度
#define LOG_PATH "/home/shxhzhxx/tcp_push/push_log.log"			//日志路径
```

linux系统上编译：
```shell
g++ -c main.cpp
g++ -c json.cpp
g++ -c push.cpp
g++ -c rb_tree.cpp
g++ main.o json.o push.o rb_tree.o -lpthread -lm
```

运行程序

客户端要连接推送服务器需要向推送服务器发起tcp连接，并通过简单的自定义协议与推送服务器交互：

Android客户端实现：

```Java
public class PushThread extends Thread {
    private static final String TAG = PushThread.class.getSimpleName();
    public static final int STATE_BIND_SUCCESS = 0;
    public static final int STATE_BIND_FAILED = 1;
    public static final int STATE_UNBIND = 2;
    public static final int STATE_CONNECTING = 3;
    private long id;
    private int num_sec = 3;
    private static final int max_sleep = 1536;
    private String host ;
    private int port;
    private Socket socket;
    private boolean auto_reconnect = false;
    private IntCallback state_callback;
    private StringCallback push_callback;

    public interface IntCallback{
        void callback(int i);
    }
    public interface StringCallback{
        void callback(String str);
    }
    public PushThread(String host,int port,long id,boolean auto_reconnect, IntCallback state_callback, StringCallback push_callback) {
        this.host=host;
        this.port=port;
        this.id = id;
        this.auto_reconnect=auto_reconnect;
        this.state_callback = state_callback;
        this.push_callback = push_callback;
    }

    @Override
    public void run() {
        String response;
        byte buff[] = new byte[512];
        int len;
        while (true) {
            try {
                socket = new Socket();
                Log.d(TAG,String.format(Locale.CHINA, "try to connect to %s (port:%d   id:%d)", host, port,id));
                state_callback.callback(STATE_CONNECTING);
                socket.connect(new InetSocketAddress(host, port), 5000);//设置连接超时5s
                socket.setKeepAlive(true);
                socket.getOutputStream().write(new JSONObject().put("device_type", 1).put("id", id).toString().getBytes());
                socket.getOutputStream().flush();
                if ((len = socket.getInputStream().read(buff)) > 0) {
                    response = new String(buff, 0, len, "UTF-8");
                    if (response.equals("200")) {
                        state_callback.callback(STATE_BIND_SUCCESS);
                        break;
                    } else {
                        Log.d(TAG,"undefined response:"+response);
                    }
                } else {
                    Log.d(TAG,"读取服务器反馈失败");
                }
            } catch (IOException e) {
                Log.d(TAG,e.getMessage());
            } catch (JSONException e) {//不应该出异常的
                Log.d(TAG, "JSONException:"+e.getMessage());
            }
            //建立连接失败
            state_callback.callback(STATE_BIND_FAILED);
            try {
                if (socket != null)
                    socket.close();
            } catch (IOException e1) {
                Log.d(TAG, "清理socket时出错：" + e1.getMessage());
            }
            if (!auto_reconnect) {//不需要自动重连，直接结束线程
                return;
            }
            Log.d(TAG, String.format(Locale.CHINA, "建立tcp连接失败,%d秒后重试", num_sec));
            try {
                Thread.sleep(num_sec * 1000);
                if (num_sec < max_sleep) {
                    num_sec *= 2;
                }
            }  catch (InterruptedException e1) {//sleep时线程被interrupt
                Log.d(TAG, "thread interrupted");
                state_callback.callback(STATE_BIND_FAILED);
                return;
            }
        }

        //连接已成功建立：
        try {
            while (socket.isConnected()) {
                if ((len = socket.getInputStream().read(buff)) > 0) {
                    try {
                        response = new String(buff, 0, len, "UTF-8");
                        push_callback.callback(response);
                    } catch (UnsupportedEncodingException ignore) {
                    }
                    socket.getOutputStream().write("200".getBytes());
                    socket.getOutputStream().flush();
                } else {
                    Log.d(TAG,"socket read return -1");
                    break;
                }
            }
        } catch (IOException e) {
            Log.d(TAG,"socket exception:"+e.getMessage());
        }
        state_callback.callback(STATE_UNBIND);
        try {
            socket.close();
        } catch (IOException e) {
            Log.d(TAG, "清理socket时出错：" + e.getMessage());
        }
    }

    @Override
    public void interrupt() {
        try {
            socket.close();
        } catch (IOException e) {
            Log.d(TAG, "清理socket时出错：" + e.getMessage());
        }
        super.interrupt();
    }
}

```
