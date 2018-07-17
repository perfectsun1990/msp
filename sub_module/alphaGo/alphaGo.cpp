
#include "alphaGo.hpp"
#include <iostream>
using namespace std;


template<class T> class MemPool
{
	struct MemNode
	{//作为分配节点存在，每个节点默认管理32*sizeof(T)的内存。
		void* _memPoint; //内存指针
		MemNode* _next;  //下一节点，和当前节点拥有一样的 对象大小*对象个数 的内存块。
		size_t _itemNum; //对象个数，_itemSize是动态推测出来的。

		MemNode(size_t itemNum) : _itemNum(itemNum), _next(NULL){
			_memPoint = malloc(sizeof(T) * _itemNum);
		}
		~MemNode(){
			free(_memPoint);
			_memPoint = NULL;
			_next = NULL;
		}
	};
protected:
	size_t _countIn;
	MemNode* _first;
	MemNode* _last;
	size_t _maxNum;			//the max size for one block
	T* _lastDelete;
protected:
	size_t _GetMemSize()
	{
		if (_last->_itemNum * 2 > _maxNum)
			return _maxNum;
		else
			return _last->_itemNum * 2;
	}
public:
	MemPool(size_t initNum = 32, size_t maxNum = 10000) : _maxNum(maxNum), _countIn(0), _lastDelete(NULL)
	{
		_first = _last = new MemNode(initNum);
	}
	~MemPool()
	{
		MemNode* cur = _first;
		while (cur)
		{
			MemNode* del = cur;
			cur = cur->_next;
			delete del;
		}
		_first = _last = NULL;
		if (_lastDelete)
			cout << "has ever destroy memory and not used" << endl;
		_lastDelete = NULL;
	}

	template<typename... Args> T* New(Args&&... args)
	{
		T* obj = NULL;
		if (_lastDelete){
			obj = _lastDelete;
			_lastDelete = *(T**)(_lastDelete);
			return new(obj)T;
		}else{
			if (_countIn >= _last->_itemNum)
			{
				MemNode* tmp = new MemNode(_GetMemSize());//申请的节点块的默认对象个数。其实就是初始还时的对象个数，。
				_last->_next = tmp;
				_last = tmp;
				_countIn = 0;
			}
			obj = (T*)((char*)_last->_memPoint + sizeof(T)*_countIn);
			_countIn++;
			return new(obj)T(std::forward<Args>(args)...);
		}
	}
	void Delete(T* del)
	{
		if (del == NULL)
			return;
		*(T**)del = _lastDelete;
		_lastDelete = del;
	}
};

//#include "MemoryPool.h"
#include <vector>
struct test
{
	test() {
		cout << "test..." << endl;
	}
	test(int a):v(a) {
		cout << "testa..." << v << endl;
	}
	int v;
};

int main()
{
	std::vector<test*> v;
	MemPool<test> mp;
#if 1
	int i;
	test* tmp;
	for (i = 0; i < 10; i++)
	{
		tmp = mp.New(i);
		cout << "create : " << tmp << endl;
		v.push_back(tmp);
	}
	cout << "===============================================" << endl;
	for (i = 0; i < 5; i++)
	{
		tmp = v.back();
		cout << "destroy : " << tmp << endl;
		mp.Delete(tmp);
		v.pop_back();
	}
	cout << "===============================================" << endl;
	for (i = 0; i < 5; i++)
	{
		cout << "CREAT SUCCESSFUL" << endl;
		tmp = mp.New();
		cout << "creat : " << tmp << endl;
		v.push_back(tmp);
	}
	cout << "===============================================" << endl;
	cout << v.size() << endl;
#endif
	system("pause");
	return 0;
}
