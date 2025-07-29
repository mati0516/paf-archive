# setup.py
from setuptools import setup, find_packages

setup(
    name='paf',
    version='0.1.0',
    packages=['paf'],
    entry_points={
        'console_scripts': [
            'paf=paf.cli:main',
        ],
    },
    author='mati',
    author_email='mati.0516@gmail.com',
    description='PAF: Parallel Archive Format tool',
    long_description='CLI for creating and extracting .paf files.',
    long_description_content_type='text/markdown',
    url='https://github.com/yourusername/paf-archive',
    classifiers=[
        'Programming Language :: Python :: 3',
        'Operating System :: OS Independent',
    ],
    python_requires='>=3.6',
)
