#ifndef WRAP_BOOST_TASK_H
#define WRAP_BOOST_TASK_H

#define BOOST_BIND_GLOBAL_PLACEHOLDERS

#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/system/error_code.hpp>

#include <memory>
#include <functional>
#include <future>
#include <queue>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <exception>
#include <stdexcept>
#include <thread>

namespace wrap_boost {

enum class TaskState {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELED
};

enum class TaskPriority {
    LOW,
    NORMAL,
    HIGH,
    CRITICAL
};

template<typename T>
class TaskResult {
public:
    TaskResult() : hasValue_(false), hasError_(false) {}
    
    explicit TaskResult(const T& value) 
        : value_(value), hasValue_(true), hasError_(false) {}
    
    explicit TaskResult(T&& value) 
        : value_(std::move(value)), hasValue_(true), hasError_(false) {}
    
    explicit TaskResult(const std::exception_ptr& error) 
        : error_(error), hasValue_(false), hasError_(true) {}
    
    bool hasValue() const { return hasValue_; }
    bool hasError() const { return hasError_; }
    
    const T& getValue() const {
        if (!hasValue_) {
            throw std::runtime_error("TaskResult has no value");
        }
        return value_;
    }
    
    T&& moveValue() {
        if (!hasValue_) {
            throw std::runtime_error("TaskResult has no value");
        }
        hasValue_ = false;
        return std::move(value_);
    }
    
    void throwIfError() const {
        if (hasError_) {
            std::rethrow_exception(error_);
        }
    }
    
    std::exception_ptr getError() const {
        return error_;
    }
    
private:
    T value_;
    std::exception_ptr error_;
    bool hasValue_;
    bool hasError_;
};

template<>
class TaskResult<void> {
public:
    TaskResult() : hasError_(false) {}
    
    explicit TaskResult(const std::exception_ptr& error) 
        : error_(error), hasError_(true) {}
    
    bool hasError() const { return hasError_; }
    
    void throwIfError() const {
        if (hasError_) {
            std::rethrow_exception(error_);
        }
    }
    
    std::exception_ptr getError() const {
        return error_;
    }
    
private:
    std::exception_ptr error_;
    bool hasError_;
};

class ITask {
public:
    using Ptr = std::shared_ptr<ITask>;
    
    virtual ~ITask() = default;
    
    virtual void execute() = 0;
    virtual void cancel() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    
    virtual uint64_t getId() const = 0;
    virtual TaskState getState() const = 0;
    virtual TaskPriority getPriority() const = 0;
    virtual void setPriority(TaskPriority priority) = 0;
    
    virtual bool isCancellable() const = 0;
    virtual bool isPausable() const = 0;
};

class TaskBase : public ITask {
public:
    TaskBase(uint64_t id, TaskPriority priority = TaskPriority::NORMAL);
    virtual ~TaskBase() = default;
    
    virtual void cancel() override;
    virtual void pause() override;
    virtual void resume() override;
    
    virtual uint64_t getId() const override;
    virtual TaskState getState() const override;
    virtual TaskPriority getPriority() const override;
    virtual void setPriority(TaskPriority priority) override;
    
    virtual bool isCancellable() const override;
    virtual bool isPausable() const override;
    
protected:
    void setState(TaskState state);
    bool checkForCancellation();
    bool checkForPause();
    
    uint64_t id_;
    TaskPriority priority_;
    std::atomic<TaskState> state_;
    std::atomic<bool> cancellationRequested_;
    std::atomic<bool> pauseRequested_;
    
    std::mutex pauseMutex_;
    std::condition_variable pauseCondition_;
    
    bool cancellable_;
    bool pausable_;
};

template<typename ReturnType>
class Task : public TaskBase {
public:
    using ResultType = TaskResult<ReturnType>;
    using TaskFunction = std::function<ReturnType()>;
    using CompletionCallback = std::function<void(const ResultType&)>;
    
    Task(uint64_t id, TaskFunction function, 
         TaskPriority priority = TaskPriority::NORMAL)
        : TaskBase(id, priority),
          function_(function),
          hasResult_(false) {}
    
    void setCompletionCallback(CompletionCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        completionCallback_ = callback;
    }
    
    ResultType getResult() const {
        std::unique_lock<std::mutex> lock(resultMutex_);
        resultCondition_.wait(lock, [this] { return hasResult_; });
        
        return result_;
    }
    
    bool hasResult() const {
        std::lock_guard<std::mutex> lock(resultMutex_);
        return hasResult_;
    }
    
    virtual void execute() override {
        if (getState() != TaskState::PENDING && getState() != TaskState::PAUSED) {
            return;
        }
        
        setState(TaskState::RUNNING);
        
        try {
            if (checkForCancellation()) {
                notifyComplete(ResultType(std::make_exception_ptr(std::runtime_error("Task cancelled"))));
                return;
            }
            
            ReturnType result = function_();
            
            if (checkForCancellation()) {
                notifyComplete(ResultType(std::make_exception_ptr(std::runtime_error("Task cancelled"))));
                return;
            }
            
            notifyComplete(ResultType(std::move(result)));
        }
        catch (...) {
            notifyComplete(ResultType(std::current_exception()));
        }
    }
    
private:
    void notifyComplete(const ResultType& taskResult) {
        setState(taskResult.hasError() ? TaskState::FAILED : TaskState::COMPLETED);
        
        {
            std::lock_guard<std::mutex> lock(resultMutex_);
            result_ = taskResult;
            hasResult_ = true;
        }
        
        resultCondition_.notify_all();
        
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (completionCallback_) {
            completionCallback_(result_);
        }
    }
    
    TaskFunction function_;
    
    mutable std::mutex resultMutex_;
    mutable std::condition_variable resultCondition_;
    ResultType result_;
    bool hasResult_;
    
    std::mutex callbackMutex_;
    CompletionCallback completionCallback_;
};

template<>
class Task<void> : public TaskBase {
public:
    using ResultType = TaskResult<void>;
    using TaskFunction = std::function<void()>;
    using CompletionCallback = std::function<void(const ResultType&)>;
    
    Task(uint64_t id, TaskFunction function, 
         TaskPriority priority = TaskPriority::NORMAL)
        : TaskBase(id, priority),
          function_(function),
          hasResult_(false) {}
    
    void setCompletionCallback(CompletionCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        completionCallback_ = callback;
    }
    
    ResultType getResult() const {
        std::unique_lock<std::mutex> lock(resultMutex_);
        resultCondition_.wait(lock, [this] { return hasResult_; });
        
        return result_;
    }
    
    bool hasResult() const {
        std::lock_guard<std::mutex> lock(resultMutex_);
        return hasResult_;
    }
    
    virtual void execute() override {
        if (getState() != TaskState::PENDING && getState() != TaskState::PAUSED) {
            return;
        }
        
        setState(TaskState::RUNNING);
        
        try {
            if (checkForCancellation()) {
                notifyComplete(ResultType(std::make_exception_ptr(std::runtime_error("Task cancelled"))));
                return;
            }
            
            function_();
            
            if (checkForCancellation()) {
                notifyComplete(ResultType(std::make_exception_ptr(std::runtime_error("Task cancelled"))));
                return;
            }
            
            notifyComplete(ResultType());
        }
        catch (...) {
            notifyComplete(ResultType(std::current_exception()));
        }
    }
    
private:
    void notifyComplete(const ResultType& taskResult) {
        setState(taskResult.hasError() ? TaskState::FAILED : TaskState::COMPLETED);
        
        {
            std::lock_guard<std::mutex> lock(resultMutex_);
            result_ = taskResult;
            hasResult_ = true;
        }
        
        resultCondition_.notify_all();
        
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (completionCallback_) {
            completionCallback_(result_);
        }
    }
    
    TaskFunction function_;
    
    mutable std::mutex resultMutex_;
    mutable std::condition_variable resultCondition_;
    ResultType result_;
    bool hasResult_;
    
    std::mutex callbackMutex_;
    CompletionCallback completionCallback_;
};

struct TaskPriorityComparator {
    bool operator()(const ITask::Ptr& a, const ITask::Ptr& b) const {
        return static_cast<int>(a->getPriority()) < static_cast<int>(b->getPriority());
    }
};

class TaskExecutor {
public:
    TaskExecutor(size_t threadCount = 0);
    ~TaskExecutor();
    
    TaskExecutor(const TaskExecutor&) = delete;
    TaskExecutor& operator=(const TaskExecutor&) = delete;
    
    void start();
    void stop();
    void waitForCompletion();
    
    uint64_t submit(ITask::Ptr task);
    
    template<typename ReturnType>
    std::shared_ptr<Task<ReturnType>> submit(
        std::function<ReturnType()> function,
        TaskPriority priority = TaskPriority::NORMAL) {
        
        uint64_t taskId = generateTaskId();
        auto task = std::make_shared<Task<ReturnType>>(taskId, function, priority);
        submit(std::static_pointer_cast<ITask>(task));
        return task;
    }
    
    bool cancelTask(uint64_t taskId);
    bool pauseTask(uint64_t taskId);
    bool resumeTask(uint64_t taskId);
    
    size_t getPendingTaskCount() const;
    size_t getRunningTaskCount() const;
    std::vector<uint64_t> getAllTaskIds() const;
    TaskState getTaskState(uint64_t taskId) const;
    
private:
    void workerThread();
    ITask::Ptr getNextTask();
    uint64_t generateTaskId();
    
    std::vector<std::thread> threads_;
    std::atomic<bool> running_;
    std::atomic<bool> stopRequested_;
    
    std::priority_queue<ITask::Ptr, std::vector<ITask::Ptr>, TaskPriorityComparator> taskQueue_;
    mutable std::mutex queueMutex_;
    std::condition_variable queueCondition_;
    
    std::map<uint64_t, ITask::Ptr> activeTasks_;
    mutable std::mutex tasksMapMutex_;
    
    std::atomic<uint64_t> nextTaskId_;
    std::atomic<size_t> runningTaskCount_;
    
    static const size_t DEFAULT_THREAD_COUNT;
};

class TaskScheduler {
public:
    using TaskFunction = std::function<void()>;
    using TaskId = uint64_t;
    
    TaskScheduler(std::shared_ptr<TaskExecutor> executor = nullptr);
    ~TaskScheduler();
    
    TaskScheduler(const TaskScheduler&) = delete;
    TaskScheduler& operator=(const TaskScheduler&) = delete;
    
    void start();
    void stop();
    
    TaskId scheduleOnce(TaskFunction function, 
                        std::chrono::milliseconds delay,
                        TaskPriority priority = TaskPriority::NORMAL);
    
    TaskId scheduleRecurring(TaskFunction function, 
                             std::chrono::milliseconds interval,
                             TaskPriority priority = TaskPriority::NORMAL);
    
    TaskId scheduleRecurring(TaskFunction function, 
                             std::chrono::milliseconds initialDelay,
                             std::chrono::milliseconds interval,
                             TaskPriority priority = TaskPriority::NORMAL);
    
    bool cancelScheduledTask(TaskId taskId);
    
    size_t getScheduledTaskCount() const;
    
private:
    struct ScheduledTask {
        TaskId id;
        TaskFunction function;
        std::chrono::steady_clock::time_point nextExecutionTime;
        std::chrono::milliseconds interval;
        TaskPriority priority;
        bool recurring;
        
        bool operator<(const ScheduledTask& other) const {
            return nextExecutionTime > other.nextExecutionTime;
        }
    };
    
    void schedulerThread();
    void executeTask(const ScheduledTask& task);
    TaskId generateTaskId();
    
    std::shared_ptr<TaskExecutor> executor_;
    mutable bool ownExecutor_;
    
    std::thread schedulerThread_;
    std::atomic<bool> running_;
    std::atomic<bool> stopRequested_;
    
    std::priority_queue<ScheduledTask> taskQueue_;
    std::map<TaskId, ScheduledTask> scheduledTasks_;
    mutable std::mutex tasksMutex_;
    std::condition_variable tasksCondition_;
    
    std::atomic<TaskId> nextTaskId_;
};

}

#endif 