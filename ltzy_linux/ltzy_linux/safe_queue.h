#ifndef __SAFE_QUEUE_H_
#define __SAFE_QUEUE_H_

#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>

#include "message_struct.h"


template <typename T>
class safe_queue
{

public:

	safe_queue() : head(new node), tail(head) {}

	~safe_queue()
	{
		node *pre;
		for (; head != tail;)
		{
			pre = head;
			head = head->next;
			delete pre;
		}
		delete tail;
	}

	T* try_pop()
	{
		node *old_head = nullptr;
		{
			std::unique_lock<std::mutex> ulkh(head_mut);
			if (head == get_tail())
				return nullptr;
			old_head = head;
			head = head->next;
		}
		T *data = old_head->data;
		delete old_head;
		return data;
	}

	T* wait_and_pop()
	{
		node *old_head = nullptr;
		{ 
			std::unique_lock<std::mutex> ulkh(head_mut);  // 锁住头
			{
				std::unique_lock<std::mutex> ulkt(tail_mut);  // 锁住尾
				data_cond.wait(ulkt, [&]()  // 等待尾部的信号
				{
					return head != tail;
				});
			}
			old_head = head;
			head = head->next;
		}
		T *data = old_head->data;
		delete old_head;
		return data;
	}

	void push(T* t)
	{
		node *new_tail = new node;
		{
			std::unique_lock<std::mutex> ulkt(tail_mut);
			tail->data = new T();
			memcpy(tail->data, t, sizeof(TranMessage));
			tail->next = new_tail;
			tail = new_tail;
		}
		data_cond.notify_one();
	}

	bool empty() {
		bool res = false;
		std::unique_lock<std::mutex> ulkh(head_mut);
		{
			std::unique_lock<std::mutex> ulkt(tail_mut);
			{
				res = (head == tail);
			}
		}
		return res;
	}

	// 遍历 要修改printf的格式化输出
	void traverse_safe_queue() {
		std::unique_lock<std::mutex> ulkh(head_mut);
		{
			std::unique_lock<std::mutex> ulkt(tail_mut);
			{
				node *t = head;
				int _num = 0;
				while (t != tail) {
					_num += 1;
					printf("safa_queuq list ele: %s %s\n", t->data->from.user_ip, t->data->message.content);  // t->data的数据类型未知
					t = t->next;
				}
				printf("totle ele: %d\n", _num);
			}
		}
	}

private:

	struct node
	{
		node() {
			data = NULL;
			next = NULL;
		}

		T *data;
		node *next;
	};

	std::mutex head_mut;
	std::mutex tail_mut;
	node *head;
	node *tail;
	std::condition_variable data_cond;


private:

	node* get_tail()
	{
		std::unique_lock<std::mutex> ulkt(tail_mut);
		return tail;
	}

};

#endif


