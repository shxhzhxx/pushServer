# pushServer
push server for web server (php etc) to push message to client


使用方法：

修改push.h的配置信息：

```c++
#define SERVER_IP_ADDRESS "192.168.2.109"  //推送服务器的ip
#define BIND_SERVER_PORT "3889"  //开放给客户端绑定推送服务器的端口
#define PUSH_SERVER_PORT "3899"  //开放给服务器端的端口
#define MAX_MESSAGE_SIZE 512  //推送信息的最大长度
#define LOG_PATH "/home/shxhzhxx/tcp_push/push_log.log"  //日志路径
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
    private String host="192.168.2.109" ;
    private int port=3889;
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
    public PushThread(long id,boolean auto_reconnect, IntCallback state_callback, StringCallback push_callback) {
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

在Service中执行PushThread，并用适当的方法处理收到的推送信息。其中绑定推送服务器时的id需要是唯一的，对于单个应用，id可以是用户的id或其他任何唯一的长整形数字，这个id
将作为推送时标识客户端的依据。如果不想在服务端生成id，可以用移动设备的IMEI或MAC地址等唯一标识。

```Java
public class MyService extends Service {
    private static final String TAG=MyService.class.getSimpleName();
    private PushThread task=null;
    private int current_state=PushThread.STATE_UNBIND;
    private PushThread.IntCallback state_callback=new PushThread.IntCallback() {
        @Override
        public void callback(int state) {
            current_state=state;
            Log.d(TAG,"current state:"+current_state);
            switch (state){
                case PushThread.STATE_CONNECTING:
                    Log.d(TAG,"正在连接推送服务器");
                    break;
                case PushThread.STATE_BIND_FAILED:
                    Log.d(TAG,"连接推送服务器失败");
                    break;
                case PushThread.STATE_BIND_SUCCESS:
                    Log.d(TAG,"连接推送服务器成功");
                    break;
                case PushThread.STATE_UNBIND:
                    Log.d(TAG,"与推送服务器断开连接");
                    if(isNetworkAvailable())
                        startWork();
                    break;
            }
        }
    };
    private PushThread.StringCallback push_callback=new PushThread.StringCallback() {
        @Override
        public void callback(String message) {
            Log.d(TAG,"message:"+message);
        }
    };
    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        if(isNetworkAvailable()){
            startWork();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        stopWork();
    }

    private void startWork(){
        long id=111;
        stopWork();
        task=new PushThread(id,false,state_callback,push_callback);
        task.start();
    }
    public void stopWork(){
        if(task!=null){
            task.interrupt();
            task=null;
        }
    }

    public boolean isNetworkAvailable() {
        ConnectivityManager connMgr = (ConnectivityManager)
                this.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo networkInfo = connMgr.getActiveNetworkInfo();
        return networkInfo != null && networkInfo.isConnected();
    }
}
```

服务端调用，以PHP为例：
```PHP
function push($id_arr,$content){
	if($fp=fsockopen("123.57.230.169",17294)){
		$data=array(
			'ids'=>$id_arr,
			'content'=>$content
		);
		if(!fwrite($fp,json_encode($data))){
			//发送失败
		}
		fclose($fp);
	}else{
		//连接失败
	}
}

push(array("128283689494703","128283689494704"),array('time'=>time(),'rand'=>rand(),'text'=>"abcde"));
```
ids指定推送目标，是json数组格式，即使推送目标只有一个，也需要像这样调用
push(array(110),array("text"=>"abcde"));
content为推送内容，格式为jsonObject

如果需要知道推送是否成功，可以在调用时增加参数：
```PHP
function push($id_arr,$content,$push_id){
	if($fp=fsockopen("123.57.230.169",17294)){
		$data=array(
			'ids'=>$id_arr,
			'content'=>$content,
			'callback_type'=>1, //0:无回调   1:url回调
			'push_id'=>$push_id,
			'callback_url'=>"http://bangumi.bilibili.com/anime/5550/",
		);
		if(!fwrite($fp,json_encode($data))){
			//发送失败
		}
		fclose($fp);
	}else{
		//连接失败
	}
}
```

推送成功时，会用get方式访问这个web连接：
```
"url?push_result=1&push_id=10&client_id=110"
```
url由callback_url参数指定，
push_result=0时推送成功，push_result=1时推送失败
push_id由push_id参数指定，
client_id为目标id