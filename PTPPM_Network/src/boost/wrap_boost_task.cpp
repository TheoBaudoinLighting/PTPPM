#include "boost/wrap_boost_task.h"
#include <iostream>
#include <thread>
#include <algorithm>

namespace wrap_boost {

const size_t TaskExecutor::DEFAULT_THREAD_COUNT = 4;

TaskBase::TaskBase(uint64_t id, TaskPriority priority)
    : id_(id),
      priority_(priority),
      state_(TaskState::PENDING),
      cancellationRequested_(false),
      pauseRequested_(false),
      cancellable_(true),
      pausable_(true) {}

void TaskBase::cancel() {
    if (!cancellable_) {
        return;
    }
    
    cancellationRequested_ = true;
    
    if (state_ == TaskState::PAUSED) {
        resume();
    }
}

void TaskBase::pause() {
    if (!pausable_ || state_ != TaskState::RUNNING) {
        return;
    }
    
    pauseRequested_ = true;
}

void TaskBase::resume() {
    if (state_ != TaskState::PAUSED) {
        return;
    }
    
    pauseRequested_ = false;
    
    {
        std::lock_guard<std::mutex> lock(pauseMutex_);
        pauseCondition_.notify_all();
    }
}

uint64_t TaskBase::getId() const {
    return id_;
}

TaskState TaskBase::getState() const {
    return state_;
}

TaskPriority TaskBase::getPriority() const {
    return priority_;
}

void TaskBase::setPriority(TaskPriority priority) {
    priority_ = priority;
}

bool TaskBase::isCancellable() const {
    return cancellable_;
}

bool TaskBase::isPausable() const {
    return pausable_;
}

void TaskBase::setState(TaskState state) {
    state_ = state;
}

bool TaskBase::checkForCancellation() {
    if (cancellationRequested_) {
        setState(TaskState::CANCELED);
        return true;
    }
    return false;
}

bool TaskBase::checkForPause() {
    if (pauseRequested_) {
        setState(TaskState::PAUSED);
        
        std::unique_lock<std::mutex> lock(pauseMutex_);
        pauseCondition_.wait(lock, [this] { 
            return !pauseRequested_ || cancellationRequested_; 
        });
        
        if (!cancellationRequested_) {
            setState(TaskState::RUNNING);
        }
        
        return cancellationRequested_;
    }
    return false;
}

TaskExecutor::TaskExecutor(size_t threadCount)
    : running_(false),
      stopRequested_(false),
      nextTaskId_(1),
      runningTaskCount_(0) {
    
    if (threadCount == 0) {
        threadCount = DEFAULT_THREAD_COUNT;
    }
    
    threads_.reserve(threadCount);
}

TaskExecutor::~TaskExecutor() {
    stop();
}

void TaskExecutor::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    stopRequested_ = false;
    
    for (size_t i = 0; i < threads_.capacity(); ++i) {
        threads_.emplace_back(&TaskExecutor::workerThread, this);
    }
}

void TaskExecutor::stop() {
    if (!running_) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        stopRequested_ = true;
        queueCondition_.notify_all();
    }
    
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    threads_.clear();
    running_ = false;
    
    {
        std::lock_guard<std::mutex> lock(tasksMapMutex_);
        
        for (auto& pair : activeTasks_) {
            pair.second->cancel();
        }
        
        activeTasks_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        
        while (!taskQueue_.empty()) {
            auto task = taskQueue_.top();
            task->cancel();
            taskQueue_.pop();
        }
    }
    
    runningTaskCount_ = 0;
}

void TaskExecutor::waitForCompletion() {
    while (true) {
        size_t pendingCount, runningCount;
        
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            pendingCount = taskQueue_.size();
        }
        
        runningCount = runningTaskCount_;
        
        if (pendingCount == 0 && runningCount == 0) {
            break;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

uint64_t TaskExecutor::submit(ITask::Ptr task) {
    if (!running_ || !task) {
        return 0;
    }
    
    uint64_t taskId = task->getId();
    
    {
        std::lock_guard<std::mutex> lock(tasksMapMutex_);
        activeTasks_[taskId] = task;
    }
    
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
        queueCondition_.notify_one();
    }
    
    return taskId;
}

bool TaskExecutor::cancelTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(tasksMapMutex_);
    
    auto it = activeTasks_.find(taskId);
    if (it != activeTasks_.end()) {
        it->second->cancel();
        return true;
    }
    
    return false;
}

bool TaskExecutor::pauseTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(tasksMapMutex_);
    
    auto it = activeTasks_.find(taskId);
    if (it != activeTasks_.end() && it->second->isPausable()) {
        it->second->pause();
        return true;
    }
    
    return false;
}

bool TaskExecutor::resumeTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(tasksMapMutex_);
    
    auto it = activeTasks_.find(taskId);
    if (it != activeTasks_.end()) {
        it->second->resume();
        return true;
    }
    
    return false;
}

size_t TaskExecutor::getPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return taskQueue_.size();
}

size_t TaskExecutor::getRunningTaskCount() const {
    return runningTaskCount_;
}

std::vector<uint64_t> TaskExecutor::getAllTaskIds() const {
    std::vector<uint64_t> result;
    
    std::lock_guard<std::mutex> lock(tasksMapMutex_);
    
    for (const auto& pair : activeTasks_) {
        result.push_back(pair.first);
    }
    
    return result;
}

TaskState TaskExecutor::getTaskState(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(tasksMapMutex_);
    
    auto it = activeTasks_.find(taskId);
    if (it != activeTasks_.end()) {
        return it->second->getState();
    }
    
    return TaskState::COMPLETED;
}

void TaskExecutor::workerThread() {
    while (running_ && !stopRequested_) {
        ITask::Ptr task = getNextTask();
        
        if (!task) {
            continue;
        }
        
        try {
            ++runningTaskCount_;
            task->execute();
        }
        catch (const std::exception& e) {
            std::cerr << "Exception during task execution: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "Unknown exception during task execution" << std::endl;
        }
        
        --runningTaskCount_;
        
        if (task->getState() == TaskState::COMPLETED || 
            task->getState() == TaskState::FAILED ||
            task->getState() == TaskState::CANCELED) {
            
            std::lock_guard<std::mutex> lock(tasksMapMutex_);
            activeTasks_.erase(task->getId());
        }
    }
}

ITask::Ptr TaskExecutor::getNextTask() {
    std::unique_lock<std::mutex> lock(queueMutex_);
    
    queueCondition_.wait(lock, [this] { 
        return !taskQueue_.empty() || stopRequested_; 
    });
    
    if (stopRequested_) {
        return nullptr;
    }
    
    if (taskQueue_.empty()) {
        return nullptr;
    }
    
    ITask::Ptr task = taskQueue_.top();
    taskQueue_.pop();
    
    return task;
}

uint64_t TaskExecutor::generateTaskId() {
    return nextTaskId_++;
}

TaskScheduler::TaskScheduler(std::shared_ptr<TaskExecutor> executor)
    : running_(false),
      stopRequested_(false),
      nextTaskId_(1) {
    
    if (executor) {
        executor_ = executor;
        ownExecutor_ = false;
    } else {
        executor_ = std::make_shared<TaskExecutor>();
        ownExecutor_ = true;
        executor_->start();
    }
}

TaskScheduler::~TaskScheduler() {
    stop();
    
    if (ownExecutor_) {
        executor_->stop();
        executor_.reset();
    }
}

void TaskScheduler::start() {
    if (running_) {
        return;
    }
    
    running_ = true;
    stopRequested_ = false;
    
    schedulerThread_ = std::thread(&TaskScheduler::schedulerThread, this);
}

void TaskScheduler::stop() {
    if (!running_) {
        return;
    }
    
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        stopRequested_ = true;
        tasksCondition_.notify_all();
    }
    
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    
    running_ = false;
    
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        scheduledTasks_.clear();
        
        while (!taskQueue_.empty()) {
            taskQueue_.pop();
        }
    }
}

TaskScheduler::TaskId TaskScheduler::scheduleOnce(
    TaskFunction function, 
    std::chrono::milliseconds delay,
    TaskPriority priority) {
    
    auto now = std::chrono::steady_clock::now();
    TaskId taskId = generateTaskId();
    
    ScheduledTask task;
    task.id = taskId;
    task.function = function;
    task.nextExecutionTime = now + delay;
    task.interval = std::chrono::milliseconds(0);
    task.priority = priority;
    task.recurring = false;
    
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        scheduledTasks_[taskId] = task;
        taskQueue_.push(task);
        tasksCondition_.notify_all();
    }
    
    return taskId;
}

TaskScheduler::TaskId TaskScheduler::scheduleRecurring(
    TaskFunction function, 
    std::chrono::milliseconds interval,
    TaskPriority priority) {
    
    return scheduleRecurring(function, interval, interval, priority);
}

TaskScheduler::TaskId TaskScheduler::scheduleRecurring(
    TaskFunction function, 
    std::chrono::milliseconds initialDelay,
    std::chrono::milliseconds interval,
    TaskPriority priority) {
    
    auto now = std::chrono::steady_clock::now();
    TaskId taskId = generateTaskId();
    
    ScheduledTask task;
    task.id = taskId;
    task.function = function;
    task.nextExecutionTime = now + initialDelay;
    task.interval = interval;
    task.priority = priority;
    task.recurring = true;
    
    {
        std::lock_guard<std::mutex> lock(tasksMutex_);
        scheduledTasks_[taskId] = task;
        taskQueue_.push(task);
        tasksCondition_.notify_all();
    }
    
    return taskId;
}

bool TaskScheduler::cancelScheduledTask(TaskId taskId) {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    
    auto it = scheduledTasks_.find(taskId);
    if (it != scheduledTasks_.end()) {
        scheduledTasks_.erase(it);
        
        std::priority_queue<ScheduledTask> newQueue;
        
        for (const auto& pair : scheduledTasks_) {
            newQueue.push(pair.second);
        }
        
        taskQueue_ = std::move(newQueue);
        
        return true;
    }
    
    return false;
}

size_t TaskScheduler::getScheduledTaskCount() const {
    std::lock_guard<std::mutex> lock(tasksMutex_);
    return scheduledTasks_.size();
}

void TaskScheduler::schedulerThread() {
    while (running_ && !stopRequested_) {
        ScheduledTask nextTask;
        bool hasTask = false;
        
        {
            std::unique_lock<std::mutex> lock(tasksMutex_);
            
            if (taskQueue_.empty()) {
                tasksCondition_.wait(lock, [this] { 
                    return !taskQueue_.empty() || stopRequested_; 
                });
                
                if (stopRequested_) {
                    break;
                }
                
                if (taskQueue_.empty()) {
                    continue;
                }
            }
            
            auto now = std::chrono::steady_clock::now();
            auto nextTime = taskQueue_.top().nextExecutionTime;
            
            if (nextTime <= now) {
                nextTask = taskQueue_.top();
                taskQueue_.pop();
                hasTask = true;
                
                if (nextTask.recurring) {
                    nextTask.nextExecutionTime = now + nextTask.interval;
                    taskQueue_.push(nextTask);
                } else {
                    scheduledTasks_.erase(nextTask.id);
                }
            } else {
                auto waitTime = nextTime - now;
                tasksCondition_.wait_for(lock, waitTime, [this, nextTime] {
                    return stopRequested_ || (!taskQueue_.empty() && taskQueue_.top().nextExecutionTime < nextTime);
                });
                
                continue;
            }
        }
        
        if (hasTask) {
            executeTask(nextTask);
        }
    }
}

void TaskScheduler::executeTask(const ScheduledTask& task) {
    executor_->submit<void>(task.function, task.priority);
}

TaskScheduler::TaskId TaskScheduler::generateTaskId() {
    return nextTaskId_++;
}

} 