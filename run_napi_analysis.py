#!/usr/bin/env python3
"""
NAPI SVF 分析工具启动脚本
分析 napi_project/HarmonyXFlowBench 路径下所有以 native 开头的文件夹
"""

import os
import sys
import subprocess
import argparse
from pathlib import Path
import time
import logging

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('napi_analysis.log'),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger(__name__)

def find_native_projects(base_path):
    """
    查找所有以native开头的项目文件夹
    
    Args:
        base_path (str): 基础路径
        
    Returns:
        list: 项目路径列表
    """
    native_projects = []
    base_dir = Path(base_path)
    
    if not base_dir.exists():
        logger.error(f"基础路径不存在: {base_path}")
        return native_projects
    
    # 查找所有以native开头的文件夹
    for item in base_dir.iterdir():
        if item.is_dir() and item.name.startswith('native'):
            native_projects.append(str(item))
            logger.info(f"找到项目: {item.name}")
    
    return native_projects

def run_svf_analysis(project_path, svf_tool_path="./napi_svf_tool"):
    """
    运行SVF分析
    
    Args:
        project_path (str): 项目路径
        svf_tool_path (str): SVF工具路径
        
    Returns:
        bool: 分析是否成功
    """
    try:
        logger.info(f"开始分析项目: {project_path}")
        
        # 检查SVF工具是否存在
        if not os.path.exists(svf_tool_path):
            logger.error(f"SVF工具不存在: {svf_tool_path}")
            return False
        
        # 构建命令
        cmd = [svf_tool_path, project_path]
        logger.info(f"执行命令: {' '.join(cmd)}")
        
        # 执行分析
        start_time = time.time()
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=3600  # 1小时超时
        )
        end_time = time.time()
        
        # 记录输出
        if result.stdout:
            logger.info(f"标准输出:\n{result.stdout}")
        if result.stderr:
            logger.warning(f"错误输出:\n{result.stderr}")
        
        # 检查结果
        if result.returncode == 0:
            logger.info(f"项目 {project_path} 分析成功，耗时: {end_time - start_time:.2f}秒")
            return True
        else:
            logger.error(f"项目 {project_path} 分析失败，返回码: {result.returncode}")
            return False
            
    except subprocess.TimeoutExpired:
        logger.error(f"项目 {project_path} 分析超时")
        return False
    except Exception as e:
        logger.error(f"项目 {project_path} 分析出错: {str(e)}")
        return False

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='NAPI SVF 分析工具启动脚本')
    parser.add_argument(
        '--base-path', 
        default='napi_project/HarmonyXFlowBench',
        help='基础路径 (默认: napi_project/HarmonyXFlowBench)'
    )
    parser.add_argument(
        '--svf-tool', 
        default='./src/napi_svf_tool',
        help='SVF工具路径 (默认: ./napi_svf_tool)'
    )
    parser.add_argument(
        '--parallel', 
        action='store_true',
        help='并行处理所有项目'
    )
    parser.add_argument(
        '--dry-run', 
        action='store_true',
        help='只显示将要分析的项目，不实际执行'
    )
    
    args = parser.parse_args()
    
    logger.info("开始NAPI SVF分析")
    logger.info(f"基础路径: {args.base_path}")
    logger.info(f"SVF工具: {args.svf_tool}")
    
    # 查找项目
    projects = find_native_projects(args.base_path)
    
    if not projects:
        logger.warning("没有找到以native开头的项目")
        return
    
    logger.info(f"找到 {len(projects)} 个项目:")
    for project in projects:
        logger.info(f"  - {project}")
    
    if args.dry_run:
        logger.info("干运行模式，不执行实际分析")
        return
    
    # 执行分析
    success_count = 0
    failed_projects = []
    
    if args.parallel:
        logger.info("使用并行模式处理项目")
        import concurrent.futures
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
            # 提交所有任务
            future_to_project = {
                executor.submit(run_svf_analysis, project, args.svf_tool): project 
                for project in projects
            }
            
            # 收集结果
            for future in concurrent.futures.as_completed(future_to_project):
                project = future_to_project[future]
                try:
                    success = future.result()
                    if success:
                        success_count += 1
                    else:
                        failed_projects.append(project)
                except Exception as e:
                    logger.error(f"项目 {project} 执行异常: {str(e)}")
                    failed_projects.append(project)
    else:
        logger.info("使用串行模式处理项目")
        for project in projects:
            success = run_svf_analysis(project, args.svf_tool)
            if success:
                success_count += 1
            else:
                failed_projects.append(project)
    
    # 输出总结
    logger.info("=" * 50)
    logger.info("分析完成总结:")
    logger.info(f"总项目数: {len(projects)}")
    logger.info(f"成功项目数: {success_count}")
    logger.info(f"失败项目数: {len(failed_projects)}")
    
    if failed_projects:
        logger.info("失败的项目:")
        for project in failed_projects:
            logger.info(f"  - {project}")
    
    logger.info("=" * 50)

if __name__ == "__main__":
    main() 