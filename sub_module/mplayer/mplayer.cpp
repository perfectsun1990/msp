
#include "pubcore.hpp"
#include "mplayer.hpp"



int32_t main()
{
	std::shared_ptr<Mplayer> mp = std::make_shared<Mplayer>("E:\\av-test\\10s.mp4");
	mp->start();
	std::this_thread::sleep_for(std::chrono::seconds(100));
	system("pause");
	return 0;
}
