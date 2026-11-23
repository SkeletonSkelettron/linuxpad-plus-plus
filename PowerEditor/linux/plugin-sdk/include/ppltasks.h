// This file is part of Notepad++ project
// Copyright (C)2026 Notepad++ Linux port contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Notepad++ Linux plugin SDK: the part of the Parallel Patterns Library (ppltasks.h of MSVC) plugins use:
// concurrency::create_task (the work runs in a thread of its own), task::wait/get/then, and cancellation tokens.

#pragma once

#include <atomic>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>

namespace concurrency
{
	class cancellation_token
	{
	public:
		cancellation_token() = default;
		explicit cancellation_token(std::shared_ptr<std::atomic<bool>> flag) : m_flag(std::move(flag)) {}
		bool is_canceled() const { return m_flag && m_flag->load(); }
		bool is_cancelable() const { return static_cast<bool>(m_flag); }
		static cancellation_token none() { return cancellation_token(); }

	private:
		std::shared_ptr<std::atomic<bool>> m_flag;
	};

	class cancellation_token_source
	{
	public:
		cancellation_token get_token() const { return cancellation_token(m_flag); }
		void cancel() const { m_flag->store(true); }

	private:
		std::shared_ptr<std::atomic<bool>> m_flag = std::make_shared<std::atomic<bool>>(false);
	};

	class task_canceled : public std::exception
	{
	public:
		const char* what() const noexcept override { return "task canceled"; }
	};

	inline void cancel_current_task() { throw task_canceled(); }

	enum task_status { not_complete, completed, canceled };

	template <typename T>
	class task
	{
	public:
		using result_type = T;
		task() = default;
		explicit task(std::shared_future<T> future) : m_future(std::move(future)) {}

		task_status wait() const
		{
			if (!m_future.valid())
				return canceled;
			m_future.wait();
			return completed;
		}
		T get() const { return m_future.get(); }
		bool is_done() const { return m_future.valid() && m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready; }

		// a continuation: runs in a thread of its own once this task is done, with its result
		template <typename F>
		auto then(F f) const
		{
			auto self = *this;
			if constexpr (std::is_invocable_v<F, T>)
			{
				using R = std::invoke_result_t<F, T>;
				return run<R>([self, f = std::move(f)]() mutable { return f(self.get()); });
			}
			else
			{
				using R = std::invoke_result_t<F, task<T>>;
				return run<R>([self, f = std::move(f)]() mutable { self.wait(); return f(self); });
			}
		}

		template <typename R, typename F>
		static task<R> run(F f)
		{
			auto promise = std::make_shared<std::promise<R>>();
			task<R> result(promise->get_future().share());
			std::thread([promise, f = std::move(f)]() mutable
			{
				try
				{
					if constexpr (std::is_void_v<R>)
					{
						f();
						promise->set_value();
					}
					else
						promise->set_value(f());
				}
				catch (...)
				{
					promise->set_exception(std::current_exception());
				}
			}).detach();
			return result;
		}

	private:
		std::shared_future<T> m_future;
	};

	template <typename F>
	auto create_task(F f)
	{
		using R = std::invoke_result_t<F>;
		return task<R>::template run<R>(std::move(f));
	}

	template <typename F>
	auto create_task(F f, cancellation_token)
	{
		return create_task(std::move(f));
	}
}

namespace Concurrency = concurrency;
namespace pplx = concurrency;
