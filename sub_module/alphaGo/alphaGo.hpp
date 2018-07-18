/************************************************************************/
/* 内存池（Memory Pool），是内存分配器（Memory Allocation）的一种表现形式。
它以预存储的方式预先分配一大块内存（相对于每次请求的内存大小来说），
使得绝大部分的内存请求只需要在已分配的大块内存上划分出一小块来即可。                                                                     */
/************************************************************************/

#pragma once

#include <iostream>

template<class T> class MemPool
{
	struct MemNode
	{//作为内存分配节点存在，每个节点默认管理32*sizeof(T)的内存。
		void* _memPoint; //内存指针
		MemNode* _next;  //下一节点，和当前节点拥有一样的 对象大小*对象个数 的内存块。
		size_t _itemNum; //对象个数

		MemNode(size_t itemNum) : _itemNum(itemNum), _next(NULL) {
			_memPoint = malloc(itemSize() * _itemNum);
		}
		~MemNode() {
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
	static inline size_t itemSize() {
		static size_t _itemSize = (sizeof(T) > sizeof(void*)) ? sizeof(T) : sizeof(void*);
		//std::cout << _itemSize << std::endl;
		return _itemSize;
	}
	size_t _GetMemSize()
	{
		return (_last->_itemNum * 2 > _maxNum) ? _maxNum : _last->_itemNum * 2;
	}
public:
	MemPool(size_t initNum = 32, size_t maxNum = 10000)
		: _maxNum(maxNum), _countIn(0), _lastDelete(nullptr)
	{
		_first = _last = new MemNode(initNum);
		std::cout << "MemPool() initNum=" << initNum << std::endl;
	}
	~MemPool()
	{
		std::cout << "~MemPool()" << std::endl;
		MemNode* cur = _first;
		while (cur)
		{
			MemNode* del = cur;
			cur = cur->_next;
			delete del;
		}
		_first = _last = NULL;
		if (_lastDelete)
			std::cout << "Warning: left destroy memory and not used" << std::endl;
		_lastDelete = NULL;
	}

	template<typename... Args> T* New(Args&&... args)
	{
		T* obj = NULL;
		if (_lastDelete) {
			obj = _lastDelete;
			_lastDelete = *(T**)(_lastDelete);//获取下一个空闲的T*指针。
			return new(obj)T;
		}
		else {
			if (_countIn >= _last->_itemNum)
			{
				MemNode* tmp = new MemNode(_GetMemSize());
				_last->_next = tmp;
				_last = tmp;
				_countIn = 0;
			}
			obj = (T*)((char*)_last->_memPoint + itemSize()*_countIn);
			_countIn++;
			return new(obj)T(std::forward<Args>(args)...);
		}
	}

	void Delete(T* ptr)
	{
		if (nullptr != ptr)
		{//注意：将_lastDelete转换成T**，再通过*取引用后获取的就是T*而非原先的T类型变量，这时就可以存储T*类型的指针了。
			ptr->~T();
			*(T**)ptr = _lastDelete;
			_lastDelete = ptr;
		}
	}
};