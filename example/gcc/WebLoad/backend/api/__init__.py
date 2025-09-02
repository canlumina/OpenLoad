#!/usr/bin/env python3
# -*- coding: utf-8 -*-

from flask import Blueprint

# 创建API蓝图
api_v1 = Blueprint('api_v1', __name__, url_prefix='/api/v1')

# 导入所有API模块
from . import device
from . import firmware  
from . import crypto_api
from . import system